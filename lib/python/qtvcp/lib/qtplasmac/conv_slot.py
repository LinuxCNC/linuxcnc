'''
conv_slot.py

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
    length = width = 0
    msg = []
    try:
        if W.lEntry.text():
            length = float(W.lEntry.text())
    except:
        msg.append(_translate('Conversational', 'LENGTH'))
    try:
        if W.wEntry.text():
            width = float(W.wEntry.text())
            radius = width / 2
    except:
        msg.append(_translate('Conversational', 'WIDTH'))
    if length > 0 and width > 0 and length >= width:
        blLength = math.sqrt((length / 2) ** 2 + width ** 2)
        blAngle = math.atan(width / (length / 2))
        length = length - width
        try:
            if W.aEntry.text():
                angle = math.radians(float(W.aEntry.text()))
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
        right = math.radians(0)
        up = math.radians(90)
        left = math.radians(180)
        down = math.radians(270)
        try:
            kOffset = float(W.kerf_width.value()) * W.kOffset.isChecked() / 2
        except:
            msg.append(_translate('Conversational', 'KERF'))
        if not W.xsEntry.text():
            W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
        text = _translate('Conversational', 'ORIGIN')
        try:
            if W.center.isChecked():
                xS = float(W.xsEntry.text()) + (width / 2) * math.cos(angle + up)
            else:
                if W.cExt.isChecked():
                    xS = (float(W.xsEntry.text()) + kOffset) + blLength * math.cos(angle + right + blAngle)
                else:
                    xS = (float(W.xsEntry.text()) - kOffset) + blLength * math.cos(angle + right + blAngle)
        except:
            msg.append('X {}'.format(text))
        if not W.ysEntry.text():
            W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
        try:
            if W.center.isChecked():
                yS = float(W.ysEntry.text()) + (width / 2) * math.sin(angle + up)
            else:
                if W.cExt.isChecked():
                    yS = (float(W.ysEntry.text()) + kOffset) + blLength * math.sin(angle + right + blAngle)
                else:
                    yS = (float(W.ysEntry.text()) - kOffset) + blLength * math.sin(angle + right + blAngle)
        except:
            msg.append('Y {}'.format(text))
        if msg:
            msg0 = _translate('Conversational', 'Invalid entry detected in')
            msg1 = ''
            for m in msg:
                msg1 += '{}\n'.format(m)
            error_set(P, '{}:\n\n{}'.format(msg0, msg1))
            return
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
        P.conv_preview_button(True)
    else:
        msg = []
        pos = False
        if length <= 0 or width <= 0:
            text = _translate('Conversational', 'A positive value is required for')
            msg.append('{}:\n\n'.format(text))
            pos = True
        if length <= 0:
            msg.append(_translate('Conversational', 'LENGTH'))
        if width <= 0:
            msg.append(_translate('Conversational', 'WIDTH'))
        if length < width:
            if pos:
                msg.append('\n\n')
            m0 = _translate('Conversational', 'LENGTH')
            m1 = _translate('Conversational', 'must be greater than or equal to')
            m2 = _translate('Conversational', 'WIDTH')
            msg.append('{} {} {}'.format(m0, m1, m2))
        if msg:
            msg0 = ''
            for m in msg:
                msg0 += '{}\n'.format(m)
            error_set(P, '{}\n'.format(msg0))
            return

def error_set(P, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Slot Error'), msg)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.lEntry.text() and W.wEntry.text():
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
        W.spLabel = QLabel(_translate('Conversational', 'START'))
        text = _translate('Conversational', 'ORIGIN')
        W.xsLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
        W.xsEntry = QLineEdit(str(P.xSaved), objectName = 'xsEntry')
        W.ysLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
        W.ysEntry = QLineEdit(str(P.ySaved), objectName = 'ysEntry')
        W.lLabel = QLabel(_translate('Conversational', 'LENGTH'))
        W.lEntry = QLineEdit()
        W.wLabel = QLabel(_translate('Conversational', 'WIDTH'))
        W.wEntry = QLineEdit()
        W.aLabel = QLabel(_translate('Conversational', 'ANGLE'))
        W.aEntry = QLineEdit('0.0', objectName='aEntry')
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING SLOT'))
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
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:P.conv_add_shape_to_file())
    W.undo.pressed.connect(lambda:P.conv_undo_shape())
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', \
               'lEntry', 'wEntry', 'aEntry']
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
        W.entries.addWidget(W.lLabel, 6, 0)
        W.entries.addWidget(W.lEntry, 6, 1)
        W.entries.addWidget(W.wLabel, 7, 0)
        W.entries.addWidget(W.wEntry, 7, 1)
        W.entries.addWidget(W.aLabel, 8, 0)
        W.entries.addWidget(W.aEntry, 8, 1)
        for r in [9,10,11]:
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
        for r in [8]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.lEntry.setFocus()
    P.convSettingsChanged = False
