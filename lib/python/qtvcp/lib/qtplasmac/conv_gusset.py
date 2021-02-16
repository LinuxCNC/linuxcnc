'''
conv_gusset.py

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
    width = height = 0
    if W.wEntry.text():
        width = float(W.wEntry.text())
    if W.hEntry.text():
        height = float(W.hEntry.text())
    if width > 0 and height > 0:
        right = math.radians(0)
        up = math.radians(90)
        left = math.radians(180)
        down = math.radians(270)
        if W.aEntry.text():
            angle = math.radians(float(W.aEntry.text()))
        else:
            angle = up
        if W.rEntry.text():
            radius = float(W.rEntry.text())
            if radius > height or radius > width:
                msg = 'Radius must be less than width and height\n\n'
                P.dialogError = True
                P.dialog_error(QMessageBox.Warning, 'GUSSET', msg)
                return
        else:
            radius = 0.0
        kOffset = float(W.kerf_width.text()) * W.kOffset.isChecked() / 2
        if not W.xsEntry.text():
            W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
        if not W.ysEntry.text():
            W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
        if W.cExt.isChecked():
            x0 = float(W.xsEntry.text()) + kOffset
            y0 = float(W.ysEntry.text()) + kOffset
        else:
            x0 = float(W.xsEntry.text()) - kOffset
            y0 = float(W.ysEntry.text()) - kOffset
        x1 = x0 + width * math.cos(right)
        y1 = y0 + width * math.sin(right)
        x2 = x0 + height * math.cos(angle)
        y2 = y0 + height * math.sin(angle)
        hypotLength = math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)
        if x2 <= x1:
            hypotAngle = left - math.atan((y2 - y1) / (x1 - x2))
        else:
            hypotAngle = right - math.atan((y2 - y1) / (x1 - x2))
        xS = x1 + (hypotLength / 2) * math.cos(hypotAngle)
        yS = y1 + (hypotLength / 2) * math.sin(hypotAngle)
        if W.liEntry.text():
            leadInOffset = math.sin(math.radians(45)) * float(W.liEntry.text())
        else:
            leadInOffset = 0
        if W.loEntry.text():
            leadOutOffset = math.sin(math.radians(45)) * float(W.loEntry.text())
        else:
            leadOutOffset = 0
        if W.cExt.isChecked():
            if y2 >= y0:
                dir = [up, right]
            else:
                dir = [down, left]
        else:
            if y2 >= y0:
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
        outTmp.write('\n(conversational gusset)\n')
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
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
            if radius > 0:
                x3 = x0 + radius
                y3 = y0
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                x4 = x0 + radius * math.cos(angle)
                y4 = y0 + radius * math.sin(angle)
                if W.rButton.text().startswith('Radius'):
                    if y2 >= y0:
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x4, y4 , x0 - x3, y0 - y3))
                    else:
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x4, y4 , x0 - x3, y0 - y3))
                else:
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x0, y0))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
        else:
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
            if radius > 0:
                x3 = x0 + radius
                y3 = y0
                x4 = x0 + radius * math.cos(angle)
                y4 = y0 + radius * math.sin(angle)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                if W.rButton.text().startswith('Radius'):
                    if y2 >= y0:
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x3, y3 , x0 - x4, y0 - y4))
                    else:
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x3, y3 , x0 - x4, y0 - y4))
                else:
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x0, y0))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
        if leadOutOffset > 0:
            if W.cExt.isChecked():
                if y2 >= y0:
                    dir = [up, left]
                else:
                    dir = [down, right]
            else:
                if y2 >= y0:
                    dir = [down, right]
                else:
                    dir = [up, left]
            xlCentre = xS + (leadOutOffset * math.cos(hypotAngle - dir[0]))
            ylCentre = yS + (leadOutOffset * math.sin(hypotAngle - dir[0]))
            xlEnd = xlCentre + (leadOutOffset * math.cos(hypotAngle - dir[1]))
            ylEnd = ylCentre + (leadOutOffset * math.sin(hypotAngle - dir[1]))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
        if W.kOffset.isChecked():
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
        if width <= 0:
            msg += 'A positive width value is required\n\n'
        if height <= 0:
            msg += 'A positive height value is required\n\n'
        P.dialogError = True
        P.dialog_error(QMessageBox.Warning, 'GUSSET', msg)

def rad_button_pressed(P, W, widget):
    if widget.text()[:3] == 'Rad':
        widget.setText('Chamfer')
    else:
        widget.setText('Radius')
    auto_preview(P, W)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.wEntry.text() and W.hEntry.text():
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
    #widgets
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
    W.xsLabel = QLabel('X ORIGIN')
    W.xsEntry = QLineEdit(objectName = 'xsEntry')
    W.ysLabel = QLabel('Y ORIGIN')
    W.ysEntry = QLineEdit(objectName = 'ysEntry')
    W.liLabel = QLabel('LEAD IN')
    W.liEntry = QLineEdit(objectName = 'liEntry')
    W.loLabel = QLabel('LEAD OUT')
    W.loEntry = QLineEdit(objectName = 'loEntry')
    W.wLabel = QLabel('WIDTH')
    W.wEntry = QLineEdit()
    W.hLabel = QLabel('HEIGHT')
    W.hEntry = QLineEdit()
    W.rButton = QPushButton('RADIUS')
    W.rEntry = QLineEdit()
    W.aLabel = QLabel('ANGLE')
    W.aEntry = QLineEdit()
    W.preview = QPushButton('PREVIEW')
    W.add = QPushButton('ADD')
    W.undo = QPushButton('UNDO')
    W.lDesc = QLabel('CREATING GUSSET')
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_gusset_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'xsLabel', 'xsEntry', 'ysLabel', 'ysEntry', \
                  'liLabel', 'liEntry', 'loLabel', 'loEntry', 'wLabel', 'wEntry', \
                  'hLabel', 'hEntry', 'rEntry', 'aLabel', 'aEntry']
    centerAlign = ['lDesc']
    rButton = ['cExt', 'cInt']
    pButton = ['preview', 'add', 'undo', 'rButton', 'kOffset']
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
    W.liEntry.setText('{}'.format(P.leadIn))
    W.loEntry.setText('{}'.format(P.leadOut))
    W.xsEntry.setText('{}'.format(P.xSaved))
    W.ysEntry.setText('{}'.format(P.ySaved))
    W.rEntry.setText('0')
    W.aEntry.setText('90.0')
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setChecked(False)
        W.kOffset.setEnabled(False)
    P.conv_undo_shape()
    #connections
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.cExt.toggled.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.rButton.pressed.connect(lambda:rad_button_pressed(P, W, W.sender()))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:undo_pressed(P, W))
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'wEntry', 'hEntry', 'rEntry', 'aEntry']
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
        W.entries.addWidget(W.xsLabel, 1, 0)
        W.entries.addWidget(W.xsEntry, 1, 1)
        W.entries.addWidget(W.ysLabel, 2, 0)
        W.entries.addWidget(W.ysEntry, 2, 1)
        W.entries.addWidget(W.liLabel, 3, 0)
        W.entries.addWidget(W.liEntry, 3, 1)
        W.entries.addWidget(W.loLabel, 4, 0)
        W.entries.addWidget(W.loEntry, 4, 1)
        W.entries.addWidget(W.wLabel, 5, 0)
        W.entries.addWidget(W.wEntry, 5, 1)
        W.entries.addWidget(W.hLabel, 6, 0)
        W.entries.addWidget(W.hEntry, 6, 1)
        W.entries.addWidget(W.rButton, 7, 0)
        W.entries.addWidget(W.rEntry, 7, 1)
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
        W.entries.addWidget(W.xsLabel, 2, 0)
        W.entries.addWidget(W.xsEntry, 2, 1)
        W.entries.addWidget(W.ysLabel, 2, 2)
        W.entries.addWidget(W.ysEntry, 2, 3)
        W.entries.addWidget(W.liLabel, 3, 0)
        W.entries.addWidget(W.liEntry, 3, 1)
        W.entries.addWidget(W.loLabel, 3, 2)
        W.entries.addWidget(W.loEntry, 3, 3)
        W.entries.addWidget(W.wLabel, 4, 0)
        W.entries.addWidget(W.wEntry, 4, 1)
        W.entries.addWidget(W.hLabel, 5, 0)
        W.entries.addWidget(W.hEntry, 5, 1)
        W.entries.addWidget(W.rButton, 6, 0)
        W.entries.addWidget(W.rEntry, 6, 1)
        W.entries.addWidget(W.aLabel, 7, 0)
        W.entries.addWidget(W.aEntry, 7, 1)
        W.s8 = QLabel('')
        W.s8.setFixedHeight(24)
        W.entries.addWidget(W.s8, 8, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.wEntry.setFocus()
