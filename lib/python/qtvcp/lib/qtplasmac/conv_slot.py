'''
conv_slot.py

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

from PyQt5.QtCore import Qt 
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap 

def preview(P, W):
    if P.dialogError: return
    length = width = 0
    if W.lEntry.text():
        length = float(W.lEntry.text())
    if W.wEntry.text():
        width = float(W.wEntry.text())
        radius = width / 2
    if length > 0 and width > 0 and length >= width:
        blLength = math.sqrt((length / 2) ** 2 + width ** 2)
        blAngle = math.atan(width / (length / 2))
        length = length - width
        if W.aEntry.text():
            angle = math.radians(float(W.aEntry.text()))
        else:
            angle = 0.0
        if W.liEntry.text():
            leadInOffset = math.sin(math.radians(45)) * float(W.liEntry.text())
        else:
            leadInOffset = 0
        if W.loEntry.text():
            leadOutOffset = math.sin(math.radians(45)) * float(W.loEntry.text())
        else:
            leadOutOffset = 0
        right = math.radians(0)
        up = math.radians(90)
        left = math.radians(180)
        down = math.radians(270)
        kOffset = float(W.kerf_width.text()) * W.kOffset.isChecked() / 2
        if not W.xsEntry.text():
            W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
        if W.center.isChecked():
            xS = float(W.xsEntry.text()) + (width / 2) * math.cos(angle + up)
        else:
            if W.cExt.isChecked():
                xS = (float(W.xsEntry.text()) + kOffset) + blLength * math.cos(angle + right + blAngle)
            else:
                xS = (float(W.xsEntry.text()) - kOffset) + blLength * math.cos(angle + right + blAngle)
        if not W.ysEntry.text():
            W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
        if W.center.isChecked():
            yS = float(W.ysEntry.text()) + (width / 2) * math.sin(angle + up)
        else:
            if W.cExt.isChecked():
                yS = (float(W.ysEntry.text()) + kOffset) + blLength * math.sin(angle + right + blAngle)
            else:
                yS = (float(W.ysEntry.text()) - kOffset) + blLength * math.sin(angle + right + blAngle)
        if W.cExt.isChecked():
            dir = [up, left, right]
        else:
            dir = [down, right, left]
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
        outTmp.write('\n(conversational slot)\n')
        outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        if leadInOffset > 0:
            xlCentre = xS + (leadInOffset * math.cos(angle + dir[0]))
            ylCentre = yS + (leadInOffset * math.sin(angle + dir[0]))
            xlStart = xlCentre + (leadInOffset * math.cos(angle + dir[1]))
            ylStart = ylCentre + (leadInOffset * math.sin(angle + dir[1]))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            if W.kOffset.isChecked():
                outTmp.write('g41.1 d#<_hal[qtplasmac.kerf_width-f]>\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
        else:
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
            outTmp.write('m3 $0 s1\n')
        x1 = xS + (length / 2) * math.cos(angle + dir[2])
        y1 = yS + (length / 2) * math.sin(angle + dir[2])
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
        if W.cExt.isChecked():
            xrCentre = x1 + (radius * math.cos(angle + down))
            yrCentre = y1 + (radius * math.sin(angle + down))
            xrEnd = xrCentre + (radius * math.cos(angle + down))
            yrEnd = yrCentre + (radius * math.sin(angle + down))
            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
            x2 = xrEnd + length * math.cos(angle + left)
            y2 = yrEnd + length * math.sin(angle + left)
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
            xrCentre = x2 + (radius * math.cos(angle + up))
            yrCentre = y2 + (radius * math.sin(angle + up))
            xrEnd = xrCentre + (radius * math.cos(angle + up))
            yrEnd = yrCentre + (radius * math.sin(angle + up))
            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
        else:
            xrCentre = x1 + (radius * math.cos(angle + down))
            yrCentre = y1 + (radius * math.sin(angle + down))
            xrEnd = xrCentre + (radius * math.cos(angle + down))
            yrEnd = yrCentre + (radius * math.sin(angle + down))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
            x2 = xrEnd + length * math.cos(angle + right)
            y2 = yrEnd + length * math.sin(angle + right)
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
            xrCentre = x2 + (radius * math.cos(angle + up))
            yrCentre = y2 + (radius * math.sin(angle + up))
            xrEnd = xrCentre + (radius * math.cos(angle + up))
            yrEnd = yrCentre + (radius * math.sin(angle + up))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
        if leadOutOffset > 0:
            if W.cExt.isChecked():
                dir = [up, right]
            else:
                dir = [down, left]
            xlCentre = xS + (leadOutOffset * math.cos(angle + dir[0]))
            ylCentre = yS + (leadOutOffset * math.sin(angle + dir[0]))
            xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[1]))
            ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[1]))
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
    else:
        msg = ''
        if length <= 0:
            msg += 'A positive Length is required\n\n'
        if width <= 0:
            msg += 'A positive Width is required\n\n'
        if length < width:
            msg += 'Length must be greater than or equal to Width'
        P.dialogError = True
        P.dialog_error(QMessageBox.Warning, 'SLOT', msg)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.lEntry.text() and W.wEntry.text():
        preview(P, W) 

def entry_changed(P, W, widget):
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setEnabled(False)
        W.kOffset.setChecked(False)
    else:
        W.kOffset.setEnabled(True)
    P.conv_entry_changed(widget)

def add_shape_to_file(P, W):
    P.conv_add_shape_to_file()

def undo_pressed(P, W):
    P.conv_undo_shape()

def widgets(P, W):
    W.ctLabel = QLabel('CUT TYPE')
    W.ctGroup = QButtonGroup(W)
    W.cExt = QRadioButton('EXTERNAL')
    W.cExt.setChecked(True)
    W.ctGroup.addButton(W.cExt)
    W.cInt = QRadioButton('INTERNAL')
    W.ctGroup.addButton(W.cInt)
    W.koLabel = QLabel('OFFSET')
    W.kOffset = QPushButton('KERF WIDTH')
    W.kOffset.setCheckable(True)
    W.spLabel = QLabel('START')
    W.spGroup = QButtonGroup(W)
    W.center = QRadioButton('CENTER')
    W.spGroup.addButton(W.center)
    W.bLeft = QRadioButton('BTM LEFT')
    W.spGroup.addButton(W.bLeft)
    W.xsLabel = QLabel('X ORIGIN')
    W.xsEntry = QLineEdit(objectName = 'xsEntry')
    W.ysLabel = QLabel('Y ORIGIN')
    W.ysEntry = QLineEdit(objectName = 'ysEntry')
    W.liLabel = QLabel('LEAD IN')
    W.liEntry = QLineEdit(objectName = 'liEntry')
    W.loLabel = QLabel('LEAD OUT')
    W.loEntry = QLineEdit(objectName = 'loEntry')
    W.lLabel = QLabel('LENGTH')
    W.lEntry = QLineEdit()
    W.wLabel = QLabel('WIDTH')
    W.wEntry = QLineEdit()
    W.aLabel = QLabel('ANGLE')
    W.aEntry = QLineEdit()
    W.preview = QPushButton('PREVIEW')
    W.add = QPushButton('ADD')
    W.undo = QPushButton('UNDO')
    W.lDesc = QLabel('CREATING SLOT')
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_slot_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'spLabel', 'xsLabel', 'xsEntry', 'ysLabel', \
                  'ysEntry', 'liLabel', 'liEntry', 'loLabel', 'loEntry', 'lLabel', \
                  'lEntry', 'wLabel', 'wEntry', 'aLabel', 'aEntry']
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
    W.undo.setEnabled(False)
    if P.oSaved:
        W.center.setChecked(True)
    else:
        W.bLeft.setChecked(True)
    W.liEntry.setText('{}'.format(P.leadIn))
    W.loEntry.setText('{}'.format(P.leadOut))
    W.xsEntry.setText('{}'.format(P.xSaved))
    W.ysEntry.setText('{}'.format(P.ySaved))
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setChecked(False)
        W.kOffset.setEnabled(False)
    W.aEntry.setText('0')
    P.conv_undo_shape()
    #connections
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.cExt.toggled.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.center.toggled.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:undo_pressed(P, W))
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', \
               'lEntry', 'wEntry', 'aEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, W.sender()))
        W[entry].editingFinished.connect(lambda:auto_preview(P, W))
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
        W.entries.addWidget(W.lLabel, 6, 0)
        W.entries.addWidget(W.lEntry, 6, 1)
        W.entries.addWidget(W.wLabel, 7, 0)
        W.entries.addWidget(W.wEntry, 7, 1)
        W.entries.addWidget(W.aLabel, 8, 0)
        W.entries.addWidget(W.aEntry, 8, 1)
        for r in range(9, 12):
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
        W.entries.addWidget(W.lLabel, 5, 0)
        W.entries.addWidget(W.lEntry, 5, 1)
        W.entries.addWidget(W.wLabel, 6, 0)
        W.entries.addWidget(W.wEntry, 6, 1)
        W.entries.addWidget(W.aLabel, 7, 0)
        W.entries.addWidget(W.aEntry, 7, 1)
        W.s8 = QLabel('')
        W.s8.setFixedHeight(24)
        W.entries.addWidget(W.s8, 8, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.lEntry.setFocus()
