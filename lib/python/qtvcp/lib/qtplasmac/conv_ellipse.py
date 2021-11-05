'''
conv_ellipse.py

Copyright (C) 2021  Phillip A Carter
Copyright (C) 2021  Gregory D Carl

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

import numpy
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

def preview(P, W):
    if P.dialogError: return
    # get width from entry box
    msg = []
    try:
        lenX = float(W.wEntry.text())
    except:
        msg.append(_translate('Conversational', 'WIDTH'))
    # get height from entry box
    try:
        lenY = float(W.hEntry.text())
    except:
        msg.append(_translate('Conversational', 'HEIGHT'))
    # get angle from entry box
    try:
        if W.aEntry.text():
            angle = numpy.radians(float(W.aEntry.text()))
        else:
            angle = 0
    except:
        msg.append(_translate('Conversational', 'ANGLE'))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, '{}:\n\n{}'.format(msg0, msg1))
        return
    # continue if valid width and height
    if lenX > 0 and lenY > 0:
        msg = []
        # if X origin blank, set to saved X origin
        if not W.xsEntry.text():
            W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
        text = _translate('Conversational', 'ORIGIN')
        # set ellipse X center
        try:
            if W.center.isChecked():
                centerX = float(W.xsEntry.text())
            else:
                centerX = float(W.xsEntry.text()) + lenX / 2
        except:
            msg.append('X {}'.format(text))
        # if Y origin blank, set to saved Y origin
        if not W.ysEntry.text():
            W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
        # set ellipse Y center
        try:
            if W.center.isChecked():
                centerY = float(W.ysEntry.text())
            else:
                centerY = float(W.ysEntry.text()) + lenY / 2
        except:
            msg.append('Y {}'.format(text))
        # adjust ellipse lengths if offsets are set
        try:
            if W.kOffset.isChecked():
                if W.cExt.isChecked():
                    lenX += float(W.kerf_width.value())
                    lenY += float(W.kerf_width.value())
                else:
                    lenX -= float(W.kerf_width.value())
                    lenY -= float(W.kerf_width.value())
        except:
            msg.append(_translate('Conversational', 'KERF'))
        # get the leadin and leadout length
        try:
            if W.liEntry.text():
                leadInOffset = numpy.sin(45) * float(W.liEntry.text())
            else:
                leadInOffset = 0
        except:
            msg.append(_translate('Conversational', 'LEAD IN'))
        try:
            if W.loEntry.text():
                leadOutOffset = numpy.sin(45) * float(W.loEntry.text())
            else:
                leadOutOffset = 0
        except:
            msg.append(_translate('Conversational', 'LEAD OUT'))
        if msg:
            msg0 = _translate('Conversational', 'Invalid entry detected in')
            msg1 = ''
            for m in msg:
                msg1 += '{}\n'.format(m)
            error_set(P, '{}:\n\n{}'.format(msg0, msg1))
            return
        # approx perimeter in mm
        perim = (numpy.pi * (3 * (lenX + lenY) - numpy.sqrt((3 * lenX + lenY) * (lenX + 3 * lenY)))) * P.unitsPerMm
        # number of points is 360 unless perimeter is > 360mm then have a segment length of 1mm
        points = 360 if perim <= 360 else int(perim)
        mult = float(360) / points
        # start/end point of cut
        start = int(points * 0.625)
        # create the ellipse points in an array
        array = numpy.linspace(0, points, points)
        X = centerX + lenX / 2 * numpy.cos(numpy.radians(array * mult))
        Y = centerY + lenY / 2 * numpy.sin(numpy.radians(array * mult))
        # rotate the ellipse if required
        for point in range(points):
            x = X[point]
            y = Y[point]
            X[point] = x * numpy.cos(angle) - y * numpy.sin(angle)
            Y[point] = x * numpy.sin(angle) + y * numpy.cos(angle)
        # initialize files
        outTmp = open(P.fTmp, 'w')
        outNgc = open(P.fNgc, 'w')
        inWiz = open(P.fNgcBkp, 'r')
        # begin writing the gcode
        for line in inWiz:
            if '(new conversational file)' in line:
                outNgc.write('\n{} (preamble)\n'.format(P.preAmble))
                break
            elif '(postamble)' in line:
                break
            elif 'm2' in line.lower() or 'm30' in line.lower():
                break
            outNgc.write(line)
        outTmp.write('\n(conversational ellipse)\n')
        outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        # get the angle of the first segment
        delta_x = -1 - 0
        delta_y = 1 - 0
        theta_radians = numpy.arctan2(delta_y, delta_x)
        if W.cExt.isChecked():
            dX = X[start - 1] - X[start]
            dY = Y[start - 1] - Y[start]
        else:
            dX = X[start] - X[start + 1]
            dY = Y[start] - Y[start + 1]
        segAngle = numpy.arctan2(dY, dX)
        # set direction constants
        right = numpy.radians(0)
        up = numpy.radians(90)
        left = numpy.radians(180)
        down = numpy.radians(270)
        # leadin points if required
        if leadInOffset > 0:
            if W.cExt.isChecked():
                dir = [up, left]
            else:
                dir = [down, right]
            xlcenter = X[start] + (leadInOffset * numpy.cos(segAngle + dir[0]))
            ylcenter = Y[start] + (leadInOffset * numpy.sin(segAngle + dir[0]))
            xlStart = xlcenter + (leadInOffset * numpy.cos(segAngle + dir[1]))
            ylStart = ylcenter + (leadInOffset * numpy.sin(segAngle + dir[1]))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(X[start], Y[start], xlcenter - xlStart, ylcenter - ylStart))
        else:
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(X[start], Y[start]))
            outTmp.write('m3 $0 s1\n')
        # write the ellipse points
        if W.cExt.isChecked():
            for point in range(start, -1, -1):
                outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
            for point in range(points - 1, start - 1, -1):
                outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
        else:
            for point in range(start, points, 1):
                outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
            for point in range(0, start + 1, 1):
                outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
        # leadout points if required
        if leadOutOffset:
            if W.cExt.isChecked():
                dir = [up, right]
            else:
                dir = [down, left]
            xlcenter = X[start] + (leadOutOffset * numpy.cos(segAngle + dir[0]))
            ylcenter = Y[start] + (leadOutOffset * numpy.sin(segAngle + dir[0]))
            xlEnd = xlcenter + (leadOutOffset * numpy.cos(segAngle + dir[1]))
            ylEnd = ylcenter + (leadOutOffset * numpy.sin(segAngle + dir[1]))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd, xlcenter - X[start], ylcenter - Y[start]))
        # finish off and close files
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
        msg0 = _translate('Conversational', 'Zero is invalid for')
        msg1 = _translate('Conversational', 'WIDTH')
        msg2 = _translate('Conversational', 'HEIGHT')
        error_set(P, '{}:\n\n{}\n{}\n'.format(msg0, msg1, msg2))

def error_set(P, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Ellipse Error'), msg)

def entry_changed(P, W, widget):
    P.conv_entry_changed(widget)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and W.wEntry.text() and W.hEntry.text():
        preview(P, W)

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
        W.wLabel = QLabel(_translate('Conversational', 'WIDTH'))
        W.wEntry = QLineEdit(objectName = '')
        W.hLabel = QLabel(_translate('Conversational', 'HEIGHT'))
        W.hEntry = QLineEdit(objectName = '')
        W.aLabel = QLabel(_translate('Conversational', 'ANGLE'))
        W.aEntry = QLineEdit('0', objectName = 'aEntry')
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING ELLIPSE'))
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_ellipse_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'spLabel', 'xsLabel', 'xsEntry', 'ysLabel', \
                  'ysEntry', 'liLabel', 'liEntry', 'loLabel', 'loEntry', 'wLabel', \
                  'wEntry', 'hLabel', 'hEntry', 'aLabel', 'aEntry']
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
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'wEntry', 'hEntry', 'aEntry']
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
        W.entries.addWidget(W.wLabel, 6, 0)
        W.entries.addWidget(W.wEntry, 6, 1)
        W.entries.addWidget(W.hLabel, 7, 0)
        W.entries.addWidget(W.hEntry, 7, 1)
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
        W.entries.addWidget(W.wLabel, 5, 0)
        W.entries.addWidget(W.wEntry, 5, 1)
        W.entries.addWidget(W.hLabel, 5, 2)
        W.entries.addWidget(W.hEntry, 5, 3)
        W.entries.addWidget(W.aLabel, 6, 0)
        W.entries.addWidget(W.aEntry, 6, 1)
        for r in [7,8]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.wEntry.setFocus()
    P.convSettingsChanged = False
