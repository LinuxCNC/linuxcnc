'''
conv_bolt.py

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
    if W.dEntry.text():
        cRadius = float(W.dEntry.text()) / 2
    else:
        cRadius = 0
    if W.hdEntry.text():
        hRadius = float(W.hdEntry.text()) / 2
    else:
        hRadius = 0
    if W.hEntry.text():
        holes = int(W.hEntry.text())
    else:
        holes = 0
    if cRadius > 0 and hRadius > 0 and holes > 0:
        if W.caEntry.text():
            cAngle = float(W.caEntry.text())
        else:
            cAngle = 360.0
        if cAngle == 360:
            hAngle = math.radians(cAngle / holes)
        else:
            hAngle = math.radians(cAngle / (holes - 1))
        ijDiff = float(W.kerf_width.text()) *W.kOffset.isChecked() / 2
        right = math.radians(0)
        up = math.radians(90)
        left = math.radians(180)
        down = math.radians(270)
        if hRadius < P.holeDiameter / 2:
            sHole = True
        else:
            sHole = False
        if W.aEntry.text():
            angle = math.radians(float(W.aEntry.text()))
        else:
            angle = 0
        if W.liEntry.text():
            leadIn = float(W.liEntry.text())
            leadInOffset = leadIn * math.sin(math.radians(45))
        else:
            leadIn = 0
            leadInOffset = 0
        if leadIn > hRadius:
            leadIn = hRadius
        if leadInOffset > hRadius:
            leadInOffset = hRadius
        if not W.xsEntry.text():
            W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
        if W.center.isChecked():
            xC = float(W.xsEntry.text())
        else:
            xC = float(W.xsEntry.text()) + cRadius
        if not W.ysEntry.text():
            W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
        if W.center.isChecked():
            yC = float(W.ysEntry.text())
        else:
            yC = float(W.ysEntry.text()) + cRadius
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
        for hole in range(holes):
            outTmp.write('\n(conversational bolt circle, hole #{})\n'.format(hole + 1))
            outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
            outTmp.write('M66 P3 L3 Q1\n')
            outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
            xhC = xC + cRadius * math.cos(hAngle * hole + angle)
            yhC = yC + cRadius * math.sin(hAngle * hole + angle)
            xS = xhC - hRadius + ijDiff
            yS = yhC
            if sHole:
                xlStart = xS + leadIn
                ylStart = yhC
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                outTmp.write('m3 $0 s1\n')
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
                outTmp.write('m67 E3 Q{}\n'.format(P.holeSpeed))
            else:
                xlCentre = xS + (leadInOffset * math.cos(angle + right))
                ylCentre = yS + (leadInOffset * math.sin(angle + right))
                xlStart = xlCentre + (leadInOffset * math.cos(angle + up))
                ylStart = ylCentre + (leadInOffset * math.sin(angle + up))
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                outTmp.write('m3 $0 s1\n')
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xlCentre - xlStart, ylCentre - ylStart))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f}\n'.format(xS, yS, hRadius - ijDiff))
            if not sHole:
                xlEnd = xlCentre + (leadInOffset * math.cos(angle + down))
                ylEnd = ylCentre + (leadInOffset * math.sin(angle + down))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd, xlCentre - xS, ylCentre - yS))
            torch = True
            if W.overcut.isChecked() and sHole:
                Torch = False
                outTmp.write('m62 p3 (disable torch)\n')
                over_cut(P, W, xS, yS, hRadius - ijDiff, hRadius - ijDiff, outTmp)
            outTmp.write('m5 $0\n')
            if sHole:
                outTmp.write('M68 E3 Q0 (reset feed rate to 100%)\n')
            if not torch:
                torch = True
                outTmp.write('m65 p3 (enable torch)\n')
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
        if cRadius == 0:
            msg += 'Diameter is required\n\n'
        if hRadius == 0:
            msg += 'Hole Diameter is required\n\n'
        if holes == 0:
            msg += '# of Holes are required'
        P.dialogError = True
        P.dialog_error(QMessageBox.Warning, 'BOLT-CIRCLE', msg)

def over_cut(P, W, lastX, lastY, IJ, radius, outTmp):
    try:
        oclength = float(W.ocEntry.text())
    except:
        oclength = 0
    centerX = lastX + IJ
    centerY = lastY
    cosA = math.cos(oclength / radius)
    sinA = math.sin(oclength / radius)
    cosB = ((lastX - centerX) / radius)
    sinB = ((lastY - centerY) / radius)
    endX = centerX + radius * ((cosB * cosA) - (sinB * sinA))
    endY = centerY + radius * ((sinB * cosA) + (cosB * sinA))
    outTmp.write('g3 x{0:.6f} y{1:.6f} i{2:.6f} j{3:.6f}\n'.format(endX, endY, IJ, 0))

def entry_changed(P, W, widget):
    P.conv_entry_changed(widget)
    # check if small hole valid
    try:
        dia = float(W.hdEntry.text())
    except:
        dia = 0
    if dia >= P.holeDiameter:
        W.overcut.setChecked(False)
        W.overcut.setEnabled(False)
        W.ocEntry.setEnabled(False)
    else:
        W.overcut.setEnabled(True)
        W.ocEntry.setEnabled(True)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.dEntry.text() and W.hdEntry.text() and W.hEntry.text():
        preview(P, W) 

def add_shape_to_file(P, W):
    P.conv_add_shape_to_file()

def undo_pressed(P, W):
    P.conv_undo_shape()

def widgets(P, W):
    #widgets
    W.overcut = QPushButton('OVER CUT')
    W.overcut.setEnabled(False)
    W.overcut.setCheckable(True)
    W.ocLabel = QLabel('OC LENGTH')
    W.ocEntry = QLineEdit(objectName = 'ocEntry')
    W.ocEntry.setEnabled(False)
    W.ocEntry.setText('{}'.format(4 * P.unitsPerMm))
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
    W.dLabel = QLabel('DIAMETER')
    W.dEntry = QLineEdit(objectName = '')
    W.hdLabel = QLabel('HOLE DIA')
    W.hdEntry = QLineEdit()
    W.hLabel = QLabel('# OF HOLES')
    W.hEntry = QLineEdit()
    W.aLabel = QLabel('ANGLE')
    W.aEntry = QLineEdit()
    W.caLabel = QLabel('CIRCLE ANG')
    W.caEntry = QLineEdit()
    W.preview = QPushButton('PREVIEW')
    W.add = QPushButton('ADD')
    W.undo = QPushButton('UNDO')
    W.lDesc = QLabel('CREATING BOLT CIRCLE')
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_bolt_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ocLabel', 'ocEntry', 'koLabel', 'spLabel', 'xsLabel', \
                  'xsEntry', 'ysLabel', 'ysEntry', 'liLabel', 'liEntry', \
                  'dLabel', 'dEntry', 'hdLabel', 'hdEntry', 'hLabel', \
                  'hEntry', 'aLabel', 'aEntry', 'caLabel', 'caEntry']
    centerAlign = ['lDesc']
    rButton = ['center', 'bLeft']
    pButton = ['preview', 'add', 'undo', 'kOffset', 'overcut']
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
    W.xsEntry.setText('{}'.format(P.xSaved))
    W.ysEntry.setText('{}'.format(P.ySaved))
    W.aEntry.setText('0')
    W.caEntry.setText('360')
    P.conv_undo_shape()
    #connections
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.center.toggled.connect(lambda:auto_preview(P, W))
    W.overcut.toggled.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:undo_pressed(P, W))
    entries = ['ocEntry', 'xsEntry', 'ysEntry', 'liEntry', 'dEntry', \
               'hdEntry', 'hEntry', 'aEntry', 'caEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, W.sender()))
        W[entry].editingFinished.connect(lambda:auto_preview(P, W))
    #add to layout
    if P.landscape:
        W.entries.addWidget(W.overcut, 0, 0)
        W.entries.addWidget(W.ocLabel, 0, 1)
        W.entries.addWidget(W.ocEntry, 0, 2)
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
        W.entries.addWidget(W.dLabel, 6, 0)
        W.entries.addWidget(W.dEntry, 6, 1)
        W.entries.addWidget(W.hdLabel, 7, 0)
        W.entries.addWidget(W.hdEntry, 7, 1)
        W.entries.addWidget(W.hLabel, 8, 0)
        W.entries.addWidget(W.hEntry, 8, 1)
        W.entries.addWidget(W.aLabel, 9, 0)
        W.entries.addWidget(W.aEntry, 9, 1)
        W.entries.addWidget(W.caLabel, 10, 0)
        W.entries.addWidget(W.caEntry, 10, 1)
        W.s11 = QLabel('')
        W.s11.setFixedHeight(24)
        W.entries.addWidget(W.s11, 11, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 2 , 2, 7, 3)
    else:
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.overcut, 1, 0)
        W.entries.addWidget(W.ocLabel, 1, 1)
        W.entries.addWidget(W.ocEntry, 1, 2)
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
        W.entries.addWidget(W.dLabel, 5, 0)
        W.entries.addWidget(W.dEntry, 5, 1)
        W.entries.addWidget(W.hdLabel, 6, 0)
        W.entries.addWidget(W.hdEntry, 6, 1)
        W.entries.addWidget(W.hLabel, 6, 2)
        W.entries.addWidget(W.hEntry, 6, 3)
        W.entries.addWidget(W.aLabel, 7, 0)
        W.entries.addWidget(W.aEntry, 7, 1)
        W.entries.addWidget(W.caLabel, 7, 2)
        W.entries.addWidget(W.caEntry, 7, 3)
        W.s8 = QLabel('')
        W.s8.setFixedHeight(24)
        W.entries.addWidget(W.s8, 8, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.dEntry.setFocus()
