'''
conv_ploygon.py

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
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QComboBox, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

def preview(P, W):
    if P.dialogError: return
    if W.sEntry.text():
        sides = int(W.sEntry.text())
    else:
        sides = 0
    try:
        if W.dEntry.text():
            data = float(W.dEntry.text())
            if W.mCombo.currentIndex() == 0:
                radius = data / 2
            elif W.mCombo.currentIndex() == 1:
                radius = (data / 2) / math.cos(math.radians(180 / sides))
            else:
                radius = data / (2 * math.sin(math.radians(180 / sides)))
        else:
            radius = 0
    except:
        msg = 'Invalid DIAMETER entry detected.\n'
        error_set(P, msg)
        return
    if sides >= 3 and radius > 0:
        msg = []
        text = _translate('Conversational', 'ORIGIN')
        ijOffset = radius * math.sin(math.radians(45))
        if not W.xsEntry.text():
            W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
        try:
            if W.center.isChecked():
                xS = float(W.xsEntry.text())
            else:
                xS = float(W.xsEntry.text()) + radius * math.cos(math.radians(0))
        except:
            msg.append(_translate('Conversational', 'X {}'.format(text)))
        if not W.ysEntry.text():
            W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
        try:
            if W.center.isChecked():
                yS = float(W.ysEntry.text())
            else:
                yS = float(W.ysEntry.text()) + radius * math.sin(math.radians(90))
        except:
            msg.append(_translate('Conversational', 'Y {}'.format(text)))
        try:
            if W.liEntry.text():
                leadInOffset = float(W.liEntry.text()) / (2 * math.pi * (90.0 / 360))
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
        try:
            if W.aEntry.text():
                sAngle = math.radians(float(W.aEntry.text()))
            else:
                sAngle = 0.0
        except:
            msg.append(_translate('Conversational', 'ANGLE'))
        if msg:
            msg0 = _translate('Conversational', 'Invalid entry detected in')
            msg1 = ''
            for m in msg:
                msg1 += '{}\n'.format(m)
            error_set(P, '{}:\n\n{}'.format(msg0, msg1))
            return
        pList = []
        for i in range(sides):
            angle = sAngle + 2 * math.pi * i / sides
            x = xS + radius * math.cos(angle)
            y = yS + radius * math.sin(angle)
            pList.append(['{:.6f}'.format(x), '{:.6f}'.format(y)])
        xCentre = (float(pList[0][0]) + float(pList[sides - 1][0])) / 2
        yCentre = (float(pList[0][1]) + float(pList[sides - 1][1])) / 2
        angle = math.atan2(float(pList[0][1]) - yCentre, float(pList[0][0]) - xCentre)
        right = math.radians(0)
        up = math.radians(90)
        left = math.radians(180)
        down = math.radians(270)
        if W.cExt.isChecked():
            dir = [down, right]
        else:
            dir = [up, left]
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
        outTmp.write('\n(conversational polygon {})\n'.format(sides))
        outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        if leadInOffset > 0:
            xlCentre = xCentre + (leadInOffset * math.cos(angle + dir[0]))
            ylCentre = yCentre + (leadInOffset * math.sin(angle + dir[0]))
            xlStart = xlCentre + (leadInOffset * math.cos(angle + dir[1]))
            ylStart = ylCentre + (leadInOffset * math.sin(angle + dir[1]))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            if W.kOffset.isChecked():
                outTmp.write('g41.1 d#<_hal[qtplasmac.kerf_width-f]>\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xCentre, yCentre, xlCentre - xlStart, ylCentre - ylStart))
        else:
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xCentre, yCentre))
            outTmp.write('m3 $0 s1\n')
        if W.cExt.isChecked():
            for i in range(sides, 0, -1):
                outTmp.write('g1 x{} y{}\n'.format(pList[i - 1][0], pList[i - 1][1]))
        else:
            for i in range(sides):
                outTmp.write('g1 x{} y{}\n'.format(pList[i][0], pList[i][1]))
        outTmp.write('g1 x{} y{}\n'.format(xCentre, yCentre))
        if leadOutOffset > 0:
            if W.cExt.isChecked():
                dir = [down, left]
            else:
                dir = [up, right]
            xlCentre = xCentre + (leadOutOffset * math.cos(angle + dir[0]))
            ylCentre = yCentre + (leadOutOffset * math.sin(angle + dir[0]))
            xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[1]))
            ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[1]))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xCentre, ylCentre - yCentre))
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
        msg = []
        if sides < 3:
            msg.append(_translate('Conversational', 'SIDES'))
        if radius <= 0:
            msg.append(_translate('Conversational', 'DIAMETER'))
        if msg:
            msg0 = _translate('Conversational', 'Invalid entry detected in')
            msg1 = ''
            for m in msg:
                msg1 += '{}\n'.format(m)
            error_set(P, '{}:\n\n{}'.format(msg0, msg1))

def error_set(P, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Polygon Error'), msg)

def mode_changed(P, W):
    if W.mCombo.currentIndex() == 2:
        W.dLabel.setText(_translate('Conversational', 'LENGTH'))
    else:
        W.dLabel.setText(_translate('Conversational', 'DIAMETER'))
    auto_preview(P, W)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.sEntry.text() and W.dEntry.text():
        preview(P, W)

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

def widgets(P, W):
    W.spGroup = QButtonGroup(W)
    W.center = QRadioButton(_translate('Conversational', 'CENTER'))
    W.spGroup.addButton(W.center)
    W.bLeft = QRadioButton(_translate('Conversational', 'BTM LEFT'))
    W.spGroup.addButton(W.bLeft)
    W.liLabel = QLabel(_translate('Conversational', 'LEAD IN'))
    W.liEntry = QLineEdit(str(P.leadIn), objectName = 'liEntry')
    W.loLabel = QLabel(_translate('Conversational', 'LEAD OUT'))
    W.loEntry = QLineEdit(str(P.leadOut), objectName = 'loEntry')
    if not P.convSettingsChanged:
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
        W.spLabel = QLabel(_translate('Conversational', 'START'))
        text = _translate('Conversational', 'ORIGIN')
        W.xsLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
        W.xsEntry = QLineEdit(str(P.xSaved), objectName = 'xsEntry')
        W.ysLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
        W.ysEntry = QLineEdit(str(P.ySaved), objectName = 'ysEntry')
        W.sLabel = QLabel(_translate('Conversational', 'SIDES'))
        W.sEntry = QLineEdit(objectName = 'intEntry')
        W.mCombo = QComboBox()
        W.dLabel = QLabel(_translate('Conversational', 'DIAMETER'))
        W.dEntry = QLineEdit()
        W.aLabel = QLabel(_translate('Conversational', 'ANGLE'))
        W.aEntry = QLineEdit('0.0', objectName='aEntry')
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING POLYGON'))
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_polygon_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'spLabel', 'xsLabel', 'xsEntry', 'ysLabel', \
                  'ysEntry', 'liLabel', 'liEntry', 'loLabel', 'loEntry', 'sLabel', \
                  'sEntry', 'dLabel', 'dEntry', 'aLabel', 'aEntry']
    centerAlign = ['lDesc']
    rButton = ['cExt', 'cInt', 'center', 'bLeft']
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
    W.mCombo.addItem('CIRCUMSCRIBED')
    W.mCombo.addItem('INSCRIBED')
    W.mCombo.addItem('SIDE LENGTH')
    if P.oSaved:
        W.center.setChecked(True)
    else:
        W.bLeft.setChecked(True)
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setChecked(False)
        W.kOffset.setEnabled(False)
    #connections
    W.preview.pressed.disconnect()
    W.undo.pressed.disconnect()
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.cExt.toggled.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.center.toggled.connect(lambda:auto_preview(P, W))
    W.mCombo.currentIndexChanged.connect(lambda:mode_changed(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:P.conv_add_shape_to_file())
    W.undo.pressed.connect(lambda:P.conv_undo_shape())
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', \
               'sEntry', 'dEntry', 'aEntry']
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
        W.entries.addWidget(W.spLabel, 1, 0)
        W.entries.addWidget(W.center, 1, 1)
        W.entries.addWidget(W.bLeft, 1, 2)
        W.entries.addWidget(W.xsLabel, 2, 0)
        W.entries.addWidget(W.xsEntry, 2, 1)
        W.entries.addWidget(W.ysLabel, 3, 0)
        W.entries.addWidget(W.ysEntry, 3, 1)
        W.entries.addWidget(W.liLabel, 4, 0)
        W.entries.addWidget(W.liEntry, 4, 1)
        W.entries.addWidget(W.loLabel, 5, 0)
        W.entries.addWidget(W.loEntry, 5, 1)
        W.entries.addWidget(W.mCombo, 6, 0, 1, 2)
        W.entries.addWidget(W.sLabel, 7, 0)
        W.entries.addWidget(W.sEntry, 7, 1)
        W.entries.addWidget(W.dLabel, 8, 0)
        W.entries.addWidget(W.dEntry, 8, 1)
        W.entries.addWidget(W.aLabel, 9, 0)
        W.entries.addWidget(W.aEntry, 9, 1)
        for r in [10,11]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
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
        W.entries.addWidget(W.spLabel, 2, 0)
        W.entries.addWidget(W.center, 2, 1)
        W.entries.addWidget(W.bLeft, 2, 2)
        W.entries.addWidget(W.xsLabel, 3, 0)
        W.entries.addWidget(W.xsEntry, 3, 1)
        W.entries.addWidget(W.ysLabel, 3, 2)
        W.entries.addWidget(W.ysEntry, 3, 3)
        W.entries.addWidget(W.liLabel, 4, 0)
        W.entries.addWidget(W.liEntry, 4, 1)
        W.entries.addWidget(W.loLabel, 4, 2)
        W.entries.addWidget(W.loEntry, 4, 3)
        W.entries.addWidget(W.mCombo, 5, 0, 1, 2)
        W.entries.addWidget(W.sLabel, 6, 0)
        W.entries.addWidget(W.sEntry, 6, 1)
        W.entries.addWidget(W.dLabel, 7, 0)
        W.entries.addWidget(W.dEntry, 7, 1)
        W.entries.addWidget(W.aLabel, 8, 0)
        W.entries.addWidget(W.aEntry, 8, 1)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.sEntry.setFocus()
    P.convSettingsChanged = False
