'''
conv_sector.py

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
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup 
from PyQt5.QtGui import QPixmap 

def preview(P, W):
    if P.dialogError: return
    msg = ''
    leadInOffset = leadOutOffset = radius = sAngle = angle = 0
    try:
        leadInOffset = math.sin(math.radians(45)) * float(W.liEntry.text())
    except:
        msg += 'Lead In\n'
    try:
        leadOutOffset = math.sin(math.radians(45)) * float(W.loEntry.text())
    except:
        msg += 'Lead Out\n'
    try:
        radius = float(W.rEntry.text())
    except:
        msg += 'Radius\n'
    try:
        sAngle = math.radians(float(W.sEntry.text()))
    except:
        msg += 'Sector Angle\n'
    try:
        angle = math.radians(float(W.aEntry.text()))
    except:
        msg += 'Angle\n'
    if msg:
        errMsg = 'Valid numerical entries required for:\n\n{}'.format(msg)
        P.dialogError = True
        P.dialog_error('SECTOR', errMsg)
        return
    if radius == 0 or sAngle == 0:
        P.conv_undo_shape('add')
        return
    if W.kOffset.isChecked() and leadInOffset <= 0:
        msg  = 'A Lead In is required if\n\n'
        msg += 'kerf width offset is enabled\n'
        P.dialogError = True
        P.dialog_error('SECTOR', msg)
        return
# set origin position
    kOffset = float(W.kerf_width.text()) * W.kOffset.isChecked() / 2
    if not W.xsEntry.text():
        W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
    if not W.ysEntry.text():
        W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
    if W.cExt.isChecked():
        xO = float(W.xsEntry.text()) + kOffset
        yO = float(W.ysEntry.text()) + kOffset
    else:
        xO = float(W.xsEntry.text()) - kOffset
        yO = float(W.ysEntry.text()) - kOffset
# set start point
    xS = xO + (radius * 0.75) * math.cos(angle)
    yS = yO + (radius * 0.75) * math.sin(angle)
# set bottom point
    xB = xO + radius * math.cos(angle)
    yB = yO + radius * math.sin(angle)
# set top point
    xT = xO + radius * math.cos(angle + sAngle)
    yT = yO + radius * math.sin(angle + sAngle)
# set directions
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    if W.cExt.isChecked():
        dir = [down, right, left, up]
    else:
        dir = [up, left, right, down]
# set leadin and leadout points
    xIC = xS + (leadInOffset * math.cos(angle + dir[0]))
    yIC = yS + (leadInOffset * math.sin(angle + dir[0]))
    xIS = xIC + (leadInOffset * math.cos(angle + dir[1]))
    yIS = yIC + (leadInOffset * math.sin(angle + dir[1]))
    xOC = xS + (leadOutOffset * math.cos(angle + dir[0]))
    yOC = yS + (leadOutOffset * math.sin(angle + dir[0]))
    xOE = xOC + (leadOutOffset * math.cos(angle + dir[2]))
    yOE = yOC + (leadOutOffset * math.sin(angle + dir[2]))
# setup files and write g-code
    outTmp = open(P.fTmp, 'w')
    outNgc = open(P.fNgc, 'w')
    inWiz = open(P.fNgcBkp, 'r')
    for line in inWiz:
        if '(new conversational file)' in line:
            outNgc.write('\n{} (preamble)\n'.format(P.preAmble))
            outNgc.write('f#<_hal[plasmac.cut-feed-rate]>\n')
            break
        elif '(postamble)' in line:
            break
        elif 'm2' in line.lower() or 'm30' in line.lower():
            break
        outNgc.write(line)
    outTmp.write('\n(conversational sector)\n')
    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xIS, yIS))
    outTmp.write('m3 $0 s1\n')
    if W.kOffset.isChecked():
        outTmp.write('g41.1 d#<_hal[qtplasmac.kerf_width-f]>\n')
    if leadInOffset:
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xIC - xIS, yIC - yIS))
    if W.cExt.isChecked():
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xO, yO))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xT, yT))
        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xB, yB, xO - xT, yO - yT))
    else:
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xB, yB))
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xT, yT, xO - xB, yO - yB))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xO, yO))
    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
    if leadOutOffset:
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xOE, yOE, xOC - xS, yOC - yS))
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

def auto_preview(P, W):
    if W.rEntry.text() and W.sEntry.text():
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

def widgets(P, W):
    #widgets
    W.ctLabel = QLabel('Cut Type')
    W.ctGroup = QButtonGroup(W)
    W.cExt = QRadioButton('External')
    W.cExt.setChecked(True)
    W.ctGroup.addButton(W.cExt)
    W.cInt = QRadioButton('Internal')
    W.ctGroup.addButton(W.cInt)
    W.koLabel = QLabel('Offset')
    W.kOffset = QPushButton('Kerf Width')
    W.kOffset.setCheckable(True)
    W.xsLabel = QLabel('X origin')
    W.xsEntry = QLineEdit(objectName = 'xsEntry')
    W.ysLabel = QLabel('Y origin')
    W.ysEntry = QLineEdit(objectName = 'ysEntry')
    W.liLabel = QLabel('Lead In')
    W.liEntry = QLineEdit(objectName = 'liEntry')
    W.loLabel = QLabel('Lead Out')
    W.loEntry = QLineEdit(objectName = 'loEntry')
    W.rLabel = QLabel('Radius')
    W.rEntry = QLineEdit()
    W.sLabel = QLabel('Sect Angle')
    W.sEntry = QLineEdit()
    W.aLabel = QLabel('Angle')
    W.aEntry = QLineEdit()
    W.preview = QPushButton('Preview')
    W.add = QPushButton('Add')
    W.undo = QPushButton('Undo')
    W.lDesc = QLabel('Creating Sector')
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_sector_l.png'.format(P.IMAGES)).scaledToWidth(240)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'xsLabel', 'xsEntry', 'ysLabel', 'ysEntry', \
                  'liLabel', 'liEntry', 'loLabel', 'loEntry', 'rLabel', 'rEntry', \
                  'sLabel', 'sEntry', 'aLabel', 'aEntry']
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
    W.liEntry.setText('{}'.format(P.leadIn))
    W.loEntry.setText('{}'.format(P.leadOut))
    W.xsEntry.setText('{}'.format(P.xSaved))
    W.ysEntry.setText('{}'.format(P.ySaved))
    W.aEntry.setText('0.0')
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setChecked(False)
        W.kOffset.setEnabled(False)
    W.rEntry.setFocus()
    P.conv_undo_shape('add')
    #connections
    W.cExt.toggled.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:P.conv_undo_shape('add'))
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'rEntry', 'sEntry', 'aEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, W.sender()))
        W[entry].editingFinished.connect(lambda:auto_preview(P, W))
    #add to layout
    W.entries.addWidget(W.ctLabel, 0, 0)
    W.entries.addWidget(W.cExt, 0, 1)
    W.entries.addWidget(W.cInt, 0, 2)
    W.entries.addWidget(W.koLabel, 0, 3)
    W.entries.addWidget(W.kOffset, 0, 4)
    W.entries.addWidget(W.xsLabel, 1, 0)
    W.entries.addWidget(W.xsEntry, 1, 1)
    W.entries.addWidget(W.ysLabel, 2, 0)
    W.entries.addWidget(W.ysEntry, 2, 1)
    W.entries.addWidget(W.liLabel, 3 , 0)
    W.entries.addWidget(W.liEntry, 3, 1)
    W.entries.addWidget(W.loLabel, 4, 0)
    W.entries.addWidget(W.loEntry, 4, 1)
    W.entries.addWidget(W.rLabel, 5, 0)
    W.entries.addWidget(W.rEntry, 5, 1)
    W.entries.addWidget(W.sLabel, 6, 0)
    W.entries.addWidget(W.sEntry, 6, 1)
    W.entries.addWidget(W.aLabel, 7, 0)
    W.entries.addWidget(W.aEntry, 7, 1)
    for blank in range(4):
        W['{}'.format(blank)] = QLabel('')
        W['{}'.format(blank)].setFixedHeight(24)
        W.entries.addWidget(W['{}'.format(blank)], 8 + blank, 0)
    W.entries.addWidget(W.preview, 12, 0)
    W.entries.addWidget(W.add, 12, 2)
    W.entries.addWidget(W.undo, 12, 4)
    W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
    W.entries.addWidget(W.iLabel, 2 , 2, 7, 3)
