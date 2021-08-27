'''
conv_line.py

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
import numpy
from shutil import copy as COPY
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QComboBox, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

def preview(P, W):
    if P.dialogError: return
    if P.conv_add_segment == 0:
        try:
            if not W.entry1.text():
                W.entry1.setText('{:0.3f}'.format(P.xOrigin))
            W.xS = float(W.entry1.text())
        except:
            msg0 = _translate('Conversational', 'Invalid X ORIGIN entry detected')
            error_set(P, '{}./n'.format(msg0))
            return
        try:
            if not W.entry2.text():
                W.entry2.setText('{:0.3f}'.format(P.yOrigin))
            W.yS = float(W.entry2.text())
        except:
            msg0 = _translate('Conversational', 'Invalid Y ORIGIN entry detected')
            error_set(P, '{}./n'.format(msg0))
            return
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
        outTmp.write('\n(conversational line)\n')
        outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(W.xS, W.yS))
        outTmp.write('m3 $0 s1\n')
        try:
            if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
                if P.landscape:
                    W.conv_savedX = W.entry4.text()
                    W.conv_savedY = W.entry5.text()
                    do_line_point_to_point(P, W, float(W.entry4.text()), float(W.entry5.text()))
                else:
                    W.conv_savedX = W.entry3.text()
                    W.conv_savedY = W.entry4.text()
                    do_line_point_to_point(P, W, float(W.entry3.text()), float(W.entry4.text()))
            elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
                if not float(W.entry4.text()):
                    raise Exception(_translate('Conversational', 'Length cannot be zero'))
                if P.landscape:
                    do_line_by_angle(P, W, float(W.entry4.text()), float(W.entry5.text()))
                else:
                    do_line_by_angle(P, W, float(W.entry3.text()), float(W.entry5.text()))
            elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
                if P.landscape:
                    W.conv_savedX = W.entry7.text()
                    W.conv_savedY = W.entry8.text()
                    do_arc_3_points(P, W, float(W.entry4.text()), float(W.entry5.text()), \
                                          float(W.entry7.text()), float(W.entry8.text()))
                else:
                    W.conv_savedX = W.entry5.text()
                    W.conv_savedY = W.entry6.text()
                    do_arc_3_points(P, W, float(W.entry3.text()), float(W.entry4.text()), \
                                          float(W.entry5.text()), float(W.entry6.text()))
            elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
                if P.landscape:
                    W.conv_savedX = W.entry4.text()
                    W.conv_savedY = W.entry5.text()
                    if float(W.entry7.text()) == 0:
                        msg0 = _translate('Conversational', 'Radius must be greater than zero')
                        error_set(P, '{}./n'.format(msg0))
                        return
                    else:
                        do_arc_2_points_radius(P, W, float(W.entry4.text()), float(W.entry5.text()), \
                                                     float(W.entry7.text()))
                else:
                    W.conv_savedX = W.entry3.text()
                    W.conv_savedY = W.entry4.text()
                    if float(W.entry5.text()) == 0:
                        msg0 = _translate('Conversational', 'Radius must be greater than zero')
                        error_set(P, '{}./n'.format(msg0))
                        return
                    else:
                        do_arc_2_points_radius(P, W, float(W.entry3.text()), float(W.entry4.text()), \
                                                     float(W.entry5.text()))
            elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
                if P.landscape:
                    if float(W.entry7.text()) == 0:
                        msg0 = _translate('Conversational', 'Radius must be greater than zero')
                        error_set(P, '{}./n'.format(msg0))
                        return
                    else:
                        do_arc_by_angle_radius(P, W, float(W.entry4.text()), float(W.entry5.text()), \
                                                     float(W.entry7.text()))
                else:
                    if float(W.entry5.text()) == 0:
                        msg0 = _translate('Conversational', 'Radius must be greater than zero')
                        error_set(P, '{}./n'.format(msg0))
                        return
                    else:
                        do_arc_by_angle_radius(P, W, float(W.entry3.text()), float(W.entry4.text()), \
                                                     float(W.entry5.text()))
        except:
            msg0 = _translate('Conversational', 'Invalid entry detected')
            error_set(P, '{}.\n'.format(msg0))
            outNgc.close()
            outTmp.close()
            return
    elif P.conv_add_segment >= 1:
        inTmp = open(P.fNgc, 'r')
        outTmp = open(P.fTmp, 'w')
        while(1):
            line = inTmp.readline()
            if not line or line == P.conv_gcodeLine:
                break
            else:
                outTmp.write(line)
        if P.conv_add_segment == 1:
            outTmp.write(line)
            while(1):
                line = inTmp.readline()
                if 'M5 $0' in line:
                    break
                else:
                    outTmp.write(line)
        inTmp.close()
        COPY(P.fTmp, P.fNgc)
        outNgc = open(P.fNgc, 'w')
        try:
            if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
                W.conv_savedX = W.entry1.text()
                W.conv_savedY = W.entry2.text()
                do_line_point_to_point(P, W, float(W.entry1.text()), float(W.entry2.text()))
            elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
                if not float(W.entry1.text()):
                    raise Exception(_translate('Conversational', 'Length cannot be zero'))
                do_line_by_angle(P, W, float(W.entry1.text()), float(W.entry2.text()))
            elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
                if P.landscape:
                    W.conv_savedX = W.entry4.text()
                    W.conv_savedY = W.entry5.text()
                    do_arc_3_points(P, W, float(W.entry1.text()), float(W.entry2.text()), \
                                          float(W.entry4.text()), float(W.entry5.text()))
                else:
                    W.conv_savedX = W.entry3.text()
                    W.conv_savedY = W.entry4.text()
                    do_arc_3_points(P, W, float(W.entry1.text()), float(W.entry2.text()), \
                                          float(W.entry3.text()), float(W.entry4.text()))
            elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
                W.conv_savedX = W.entry1.text()
                W.conv_savedY = W.entry2.text()
                if P.landscape:
                    if float(W.entry4.text()) == 0:
                        msg0 = _translate('Conversational', 'Radius must be greater than zero')
                        error_set(P, '{}./n'.format(msg0))
                        return
                    else:
                        do_arc_2_points_radius(P, W, float(W.entry1.text()), float(W.entry2.text()), \
                                                     float(W.entry4.text()))
                else:
                    if float(W.entry3.text()) == 0:
                        msg0 = _translate('Conversational', 'Radius must be greater than zero')
                        error_set(P, '{}./n'.format(msg0))
                        return
                    else:
                        do_arc_2_points_radius(P, W, float(W.entry1.text()), float(W.entry2.text()), \
                                                     float(W.entry3.text()))
            elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
                if float(W.entry3.text()) == 0:
                    msg0 = _translate('Conversational', 'Radius must be greater than zero')
                    error_set(P, '{}./n'.format(msg0))
                    return
                else:
                    do_arc_by_angle_radius(P, W, float(W.entry1.text()), float(W.entry2.text()), \
                                                 float(W.entry3.text()))
        except:
            msg0 = _translate('Conversational', 'Invalid entry detected')
            error_set(P, '{}.\n'.format(msg0))
            outNgc.close()
            outTmp.close()
            return
    outTmp.write(P.conv_gcodeLine)
    outTmp.write('M5 $0\n')
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
    if P.conv_add_segment == 1:
        P.conv_add_segment = 2

def error_set(P, error):
    P.conv_undo_shape()
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Line Error'), error)

def do_line_point_to_point(P, W, inX, inY):
    W.xE = inX
    W.yE = inY
    P.conv_gcodeLine = 'g1 x{:.6f} y{:.6f}\n'.format(W.xE, W.yE)

def do_line_by_angle(P, W, inL, inA):
    angle = math.radians(inA)
    W.xE = W.xS + (inL * math.cos(angle))
    W.yE = W.yS + (inL * math.sin(angle))
    P.conv_gcodeLine = 'g1 x{:.6f} y{:.6f}\n'.format(W.xE, W.yE)
    W.conv_savedX = str(W.xE)
    W.conv_savedY = str(W.yE)

def do_arc_3_points(P, W, inX1, inY1, inXE, inYE):
    W.xE = inXE
    W.yE = inYE
    A = numpy.array([W.xS, W.yS, 0.0])
    B = numpy.array([inX1, inY1, 0.0])
    C = numpy.array([inXE, inYE, 0.0])
    a = numpy.linalg.norm(C - B)
    b = numpy.linalg.norm(C - A)
    c = numpy.linalg.norm(B - A)
    s = (a + b + c) / 2
    R = a*b*c / 4 / numpy.sqrt(s * (s - a) * (s - b) * (s - c))
    b1 = a*a * (b*b + c*c - a*a)
    b2 = b*b * (a*a + c*c - b*b)
    b3 = c*c * (a*a + b*b - c*c)
    p = numpy.column_stack((A, B, C)).dot(numpy.hstack((b1, b2, b3)))
    p /= b1 + b2 + b3
    G = '3' if (inX1-W.xS)*(W.yE-W.yS)-(inY1-W.yS)*(W.xE-W.xS) > 0 else '2'
    P.conv_gcodeLine = 'g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(G, W.xE, W.yE, p[0] - W.xS, p[1] - W.yS)

def do_arc_2_points_radius(P, W, inXE, inYE, radius):
    W.xE = inXE
    W.yE = inYE
    dir = math.radians(270) if radius > 0 else math.radians(90)
    arcType = '2' if W.g2Arc.isChecked() else '3'
    height = math.sqrt((W.xE - W.xS) ** 2 + (W.yE - W.yS) ** 2) * 0.5
    length = math.sqrt((radius ** 2) - (height ** 2))
    angle = math.atan2((W.yE - W.yS), (W.xE - W.xS))
    xLineCentre = (W.xS + W.xE) / 2
    yLineCentre = (W.yS + W.yE) / 2
    xArcCentre = xLineCentre + length * math.cos(angle + dir)
    yArcCentre = yLineCentre + length * math.sin(angle + dir)
    P.conv_gcodeLine = ('g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(arcType, W.xE, W.yE, xArcCentre - W.xS, yArcCentre - W.yS))

def do_arc_by_angle_radius(P, W, inL, inA, inR):
    angle = math.radians(inA)
    xE = W.xS + (inL * math.cos(angle))
    yE = W.yS + (inL * math.sin(angle))
    W.conv_savedX = str(xE)
    W.conv_savedY = str(yE)
    do_arc_2_points_radius(P, W, xE, yE, inR)

def set_line_point_to_point(P, W):
    P.lType = None
    W.iLabel.setPixmap(W.pixLinePoint)
    if P.conv_add_segment > 0:
        text = _translate('Conversational', 'END')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('')
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('')
        W.label3.setText('')
        W.entry3.hide()
        W.label4.setText('')
        W.entry4.hide()
        W.label5.setText('')
        W.entry5.hide()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.g3Arc.hide()
        W.g2Arc.hide()
        W.entry1.setFocus()
    else:
        text = _translate('Conversational', 'START')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('{:0.3f}'.format(0))
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('{:0.3f}'.format(0))
        text = _translate('Conversational', 'END')
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
        else:
            W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText('')
            W.entry5.hide()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.g3Arc.hide()
        W.g2Arc.hide()
        if P.landscape:
            W.entry4.setFocus()
        else:
            W.entry3.setFocus()

def set_line_by_angle(P, W):
    P.lType = None
    W.iLabel.setPixmap(W.pixLineAngle)
    if P.conv_add_segment > 0:
        W.label1.setText(_translate('Conversational', 'LENGTH'))
        W.entry1.setText('')
        W.label2.setText(_translate('Conversational', 'ANGLE'))
        W.entry2.setText('0')
        W.label3.setText('')
        W.entry3.hide()
        W.label4.setText('')
        W.entry4.hide()
        W.label5.setText('')
        W.entry5.hide()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.g3Arc.hide()
        W.g2Arc.hide()
        W.entry1.setFocus()
    else:
        text = _translate('Conversational', 'START')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('{:0.3f}'.format(0))
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('{:0.3f}'.format(0))
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'LENGTH'))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'ANGLE'))
            W.label5.show()
            W.entry5.setText('0')
            W.entry5.show()
        else:
            W.label3.setText(_translate('Conversational', 'LENGTH'))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText('')
            W.entry4.hide()
            W.label5.setText(_translate('Conversational', 'ANGLE'))
            W.label5.show()
            W.entry5.setText('0')
            W.entry5.show()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.g3Arc.hide()
        W.g2Arc.hide()
        if P.landscape:
            W.entry4.setFocus()
        else:
            W.entry3.setFocus()

def set_arc_3_points(P, W):
    P.lType = None
    W.iLabel.setPixmap(W.pixArc3p)
    if P.conv_add_segment > 0:
        text = _translate('Conversational', 'NEXT')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('')
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('')
        text = _translate('Conversational', 'END')
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
        else:
            W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText('')
            W.entry5.hide()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.g3Arc.hide()
        W.g2Arc.hide()
        W.entry1.setFocus()
    else:
        text = _translate('Conversational', 'START')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('{:0.3f}'.format(0))
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('{:0.3f}'.format(0))
        text = _translate('Conversational', 'NEXT')
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
            W.label7.setText('')
            W.entry7.hide()
            text = _translate('Conversational', 'END')
            W.label7.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label7.show()
            W.entry7.setText('')
            W.entry7.show()
            W.label8.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label8.show()
            W.entry8.setText('')
            W.entry8.show()
        else:
            W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            text = _translate('Conversational', 'END')
            W.label5.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
            W.label6.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label6.show()
            W.entry6.setText('')
            W.entry6.show()
            W.label7.setText('')
            W.entry7.hide()
            W.label8.setText('')
            W.entry8.hide()
        W.g3Arc.hide()
        W.g2Arc.hide()
        if P.landscape:
            W.entry4.setFocus()
        else:
            W.entry3.setFocus()

def set_arc_2_points_radius(P, W):
    P.lType = 'a2pr'
    W.iLabel.setPixmap(W.pixArc2pr)
    if P.conv_add_segment > 0:
        text = _translate('Conversational', 'END')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('')
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('')
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'RADIUS'))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
        else:
            W.label3.setText(_translate('Conversational', 'RADIUS'))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText('')
            W.entry4.hide()
        W.label5.setText('')
        W.entry5.hide()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry7.hide()
        W.entries.removeWidget(W.g3Arc)
        W.entries.removeWidget(W.g2Arc)
        if P.landscape:
            W.entries.addWidget(W.g2Arc, 6, 0)
            W.entries.addWidget(W.g3Arc, 6, 1)
        else:
            W.entries.addWidget(W.g2Arc, 4, 3)
            W.entries.addWidget(W.g3Arc, 4, 4)
        W.g3Arc.show()
        W.g2Arc.show()
        W.entry1.setFocus()
    else:
        text = _translate('Conversational', 'START')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('{:0.3f}'.format(0))
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('{:0.3f}'.format(0))
        text = _translate('Conversational', 'END')
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
            W.label6.setText('')
            W.entry6.hide()
            W.label7.setText(_translate('Conversational', 'RADIUS'))
            W.label7.show()
            W.entry7.setText('')
            W.entry7.show()
        else:
            W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'RADIUS'))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
            W.label6.setText('')
            W.entry6.hide()
            W.label7.setText('')
            W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.entries.removeWidget(W.g3Arc)
        W.entries.removeWidget(W.g2Arc)
        if P.landscape:
            W.entries.addWidget(W.g2Arc, 9, 0)
            W.entries.addWidget(W.g3Arc, 9, 1)
        else:
            W.entries.addWidget(W.g2Arc, 6, 3)
            W.entries.addWidget(W.g3Arc, 6, 4)
        W.g3Arc.show()
        W.g2Arc.show()
        if P.landscape:
            W.entry4.setFocus()
        else:
            W.entry3.setFocus()

def set_arc_by_angle_radius(P, W):
    P.lType = 'abar'
    W.iLabel.setPixmap(W.pixArcAngle)
    if P.conv_add_segment > 0:
        W.label1.setText(_translate('Conversational', 'LENGTH'))
        W.entry1.setText('')
        W.label2.setText(_translate('Conversational', 'ANGLE'))
        W.entry2.setText('0')
        W.label3.setText(_translate('Conversational', 'RADIUS'))
        W.entry3.setText('')
        W.label3.show()
        W.entry3.show()
        W.label4.setText('')
        W.entry4.hide()
        W.label5.setText('')
        W.entry5.hide()
        W.label6.setText('')
        W.entry6.hide()
        W.label7.setText('')
        W.entry7.hide()
        W.label8.setText('')
        W.entry7.hide()
        W.entries.removeWidget(W.g3Arc)
        W.entries.removeWidget(W.g2Arc)
        if P.landscape:
            W.entries.addWidget(W.g2Arc, 4, 0)
            W.entries.addWidget(W.g3Arc, 4, 1)
        else:
            W.entries.addWidget(W.g2Arc, 4, 3)
            W.entries.addWidget(W.g3Arc, 4, 4)
        W.g3Arc.show()
        W.g2Arc.show()
        W.entry1.setFocus()
    else:
        text = _translate('Conversational', 'START')
        W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
        W.entry1.setText('{:0.3f}'.format(0))
        W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.entry2.setText('{:0.3f}'.format(0))
        if P.landscape:
            W.label3.setText('')
            W.entry3.hide()
            W.label4.setText(_translate('Conversational', 'LENGTH'))
            W.label4.show()
            W.entry4.setText('')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'ANGLE'))
            W.label5.show()
            W.entry5.setText('0')
            W.entry5.show()
            W.label6.setText('')
            W.entry6.hide()
            W.label7.setText(_translate('Conversational', 'RADIUS'))
            W.label7.show()
            W.entry7.setText('')
            W.entry7.show()
        else:
            W.label3.setText(_translate('Conversational', 'LENGTH'))
            W.label3.show()
            W.entry3.setText('')
            W.entry3.show()
            W.label4.setText(_translate('Conversational', 'ANGLE'))
            W.label4.show()
            W.entry4.setText('0')
            W.entry4.show()
            W.label5.setText(_translate('Conversational', 'RADIUS'))
            W.label5.show()
            W.entry5.setText('')
            W.entry5.show()
            W.label6.setText('')
            W.entry6.hide()
            W.label7.setText('')
            W.entry7.hide()
        W.label8.setText('')
        W.entry8.hide()
        W.entries.removeWidget(W.g3Arc)
        W.entries.removeWidget(W.g2Arc)
        if P.landscape:
            W.entries.addWidget(W.g2Arc, 9, 0)
            W.entries.addWidget(W.g3Arc, 9, 1)
        else:
            W.entries.addWidget(W.g2Arc, 6, 3)
            W.entries.addWidget(W.g3Arc, 6, 4)
        W.g3Arc.show()
        W.g2Arc.show()
        if P.landscape:
            W.entry4.setFocus()
        else:
            W.entry3.setFocus()

def line_type_changed(P, W):
    if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
        set_line_point_to_point(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
        set_line_by_angle(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
        set_arc_3_points(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
        set_arc_2_points_radius(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
        set_arc_by_angle_radius(P, W)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.entry1.text() and W.entry2.text():
        if (P.conv_add_segment == 0):
            if P.landscape:
                if (W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT') and W.entry4.text() and W.entry5.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE') and W.entry4.text() and W.entry5.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 3P') and W.entry4.text() and W.entry5.text() and W.entry7.text() and   W.entry8.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS') and W.entry4.text() and W.entry5.text() and W.entry7.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS') and W.entry4.text() and W.entry5.text() and W.entry7.text()):
                    preview(P, W)
            else:
                if (W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT') and W.entry3.text() and W.entry4.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE') and W.entry3.text() and W.entry5.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 3P') and W.entry3.text() and W.entry4.text() and W.entry5.text() and W.entry6.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS') and W.entry3.text() and W.entry4.text() and W.entry5.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS') and W.entry3.text() and W.entry4.text() and W.entry5.text()):
                    preview(P, W)
        else:
            if P.landscape:
                if (W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT')) or \
                   (W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE')) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 3P') and W.entry4.text() and W.entry5.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS') and W.entry4.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS') and W.entry3.text()):
                    preview(P, W)
            else:
                if (W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT')) or \
                   (W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE')) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 3P') and W.entry3.text() and W.entry4.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS') and W.entry3.text()) or \
                   (W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS') and W.entry3.text()):
                    preview(P, W)

def add_shape_to_file(P, W):
    P.conv_gcodeSave = P.conv_gcodeLine
    P.conv_add_shape_to_file()
    P.conv_add_segment = 0
    line_type_changed(P, W)
    W.continu.setEnabled(True)

def continu_shape(P, W):
    W.xS = W.xE
    W.yS = W.yE
    P.conv_add_segment = 1
    line_type_changed(P, W)
    W.continu.setEnabled(False)
    W.add.setEnabled(False)

def undo_shape(P, W):
    P.conv_undo_shape()
    P.conv_add_segment = 0
    line_type_changed(P, W)
    if len(P.conv_gcodeSave):
        P.conv_gcodeLine = P.conv_gcodeSave

def entry_changed(P, W, widget, entry):
    if (P.lType == 'a2pr' and P.conv_add_segment >= 1 and P.landscape and widget == W.entry4) or \
       (P.lType == 'a2pr' and P.conv_add_segment >= 1 and not P.landscape and widget == W.entry3) or \
       (P.lType == 'a2pr' and P.conv_add_segment == 0 and P.landscape and widget == W.entry7) or \
       (P.lType == 'a2pr' and P.conv_add_segment == 0 and not P.landscape and widget == W.entry5) or \
       (P.lType == 'abar' and P.conv_add_segment >= 1 and widget == W.entry3) or \
       (P.lType == 'abar' and P.conv_add_segment == 0 and P.landscape and widget == W.entry7) or \
       (P.lType == 'abar' and P.conv_add_segment == 0 and not P.landscape and widget == W.entry5):
        widget.setObjectName(None)
    else:
        widget.setObjectName('aEntry')
    P.conv_entry_changed(widget)

def widgets(P, W):
    #widgets
    W.lType = QComboBox()
    W.label1 = QLabel()
    W.entry1 = QLineEdit()
    W.label2 = QLabel()
    W.entry2 = QLineEdit()
    W.label3 = QLabel()
    W.entry3 = QLineEdit()
    W.label4 = QLabel()
    W.entry4 = QLineEdit()
    W.label5 = QLabel()
    W.entry5 = QLineEdit()
    W.label6 = QLabel()
    W.entry6 = QLineEdit()
    W.label7 = QLabel()
    W.entry7 = QLineEdit()
    W.label8 = QLabel()
    W.entry8 = QLineEdit()
    W.preview = QPushButton(_translate('Conversational', 'PREVIEW'))
    W.continu = QPushButton(_translate('Conversational', 'CONTINUE'))
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.undo = QPushButton(_translate('Conversational', 'UNDO'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING LINE OR ARC'))
    text = _translate('Conversational', 'CW')
    W.g2Arc = QRadioButton('      {}'.format(text))
    text = _translate('Conversational', 'CCW')
    W.g3Arc = QRadioButton('     {}'.format(text))
    W.iLabel = QLabel()
    W.pixLinePoint = QPixmap('{}conv_line_point.png'.format(P.IMAGES)).scaledToWidth(196)
    W.pixLineAngle = QPixmap('{}conv_line_angle.png'.format(P.IMAGES)).scaledToWidth(196)
    W.pixArc3p = QPixmap('{}conv_arc_3p.png'.format(P.IMAGES)).scaledToWidth(196)
    W.pixArc2pr = QPixmap('{}conv_arc_2pr.png'.format(P.IMAGES)).scaledToWidth(196)
    W.pixArcAngle = QPixmap('{}conv_arc_angle.png'.format(P.IMAGES)).scaledToWidth(196)
    #alignment and size
    rightAlign = ['label1', 'entry1', 'label2', 'entry2', 'label3', 'entry3', \
                  'label4', 'entry4', 'label5', 'entry5', 'label6', 'entry6', \
                  'label7', 'entry7', 'label8', 'entry8']
    centerAlign = ['lDesc']
    rButton = ['g2Arc', 'g3Arc']
    pButton = ['preview', 'continu', 'add', 'undo']
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
    W.continu.setEnabled(False)
    W.lType.addItem(_translate('Conversational', 'LINE POINT ~ POINT'))
    W.lType.addItem(_translate('Conversational', 'LINE BY ANGLE'))
    W.lType.addItem(_translate('Conversational', 'ARC 3P'))
    W.lType.addItem(_translate('Conversational', 'ARC 2P +RADIUS'))
    W.lType.addItem(_translate('Conversational', 'ARC ANGLE +RADIUS'))
    P.conv_add_segment = 0
    P.conv_gcodeLine = ''
    P.conv_gcodeSave = ''
    W.conv_savedX = ''
    W.conv_savedY = ''
    if not W.g2Arc.isChecked() and not W.g3Arc.isChecked():
        W.g2Arc.setChecked(True)
    P.conv_undo_shape()
    #connections
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.continu.pressed.connect(lambda:continu_shape(P, W))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:undo_shape(P, W))
    W.lType.currentTextChanged.connect(lambda:line_type_changed(P, W))
    W.g2Arc.toggled.connect(lambda:auto_preview(P, W))
    entries = ['entry1', 'entry2', 'entry3', 'entry4',
               'entry5', 'entry6', 'entry7', 'entry8']
    for entry in entries:
        W[entry].textChanged.connect(lambda w:entry_changed(P, W, W.sender(), w))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        for row in range (14):
            W.entries.setRowMinimumHeight(row, 24)
        W.entries.addWidget(W.lType, 0, 0, 1, 2)
        W.entries.addWidget(W.label1, 1, 0)
        W.entries.addWidget(W.entry1, 1, 1)
        W.entries.addWidget(W.label2, 2, 0)
        W.entries.addWidget(W.entry2, 2, 1)
        W.entries.addWidget(W.label3, 3, 0)
        W.entries.addWidget(W.entry3, 3, 1)
        W.entries.addWidget(W.label4, 4, 0)
        W.entries.addWidget(W.entry4, 4, 1)
        W.entries.addWidget(W.label5, 5, 0)
        W.entries.addWidget(W.entry5, 5, 1)
        W.entries.addWidget(W.label6, 6, 0)
        W.entries.addWidget(W.entry6, 6, 1)
        W.entries.addWidget(W.label7, 7, 0)
        W.entries.addWidget(W.entry7, 7, 1)
        W.entries.addWidget(W.label8, 8, 0)
        W.entries.addWidget(W.entry8, 8, 1)
        for r in range(9, 12):
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.continu, 12, 1)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 2 , 2, 7, 3)
    else:
        for row in range (11):
            W.entries.setRowMinimumHeight(row, 24)
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.lType, 1, 0, 1, 2)
        W.entries.addWidget(W.label1, 2, 0)
        W.entries.addWidget(W.entry1, 2, 1)
        W.entries.addWidget(W.label2, 2, 2)
        W.entries.addWidget(W.entry2, 2, 3)
        W.s3 = QLabel('')
        W.s3.setFixedHeight(24)
        W.entries.addWidget(W.s3, 3, 0)
        W.entries.addWidget(W.label3, 4, 0)
        W.entries.addWidget(W.entry3, 4, 1)
        W.entries.addWidget(W.label4, 4, 2)
        W.entries.addWidget(W.entry4, 4, 3)
        W.s5 = QLabel('')
        W.s5.setFixedHeight(24)
        W.entries.addWidget(W.s5, 5, 0)
        W.entries.addWidget(W.label5, 6, 0)
        W.entries.addWidget(W.entry5, 6, 1)
        W.entries.addWidget(W.label6, 6, 2)
        W.entries.addWidget(W.entry6, 6, 3)
        W.s7 = QLabel('')
        W.s7.setFixedHeight(24)
        W.entries.addWidget(W.s7, 7, 0)
        W.entries.addWidget(W.label7, 8, 0)
        W.entries.addWidget(W.entry7, 8, 1)
        W.entries.addWidget(W.label8, 8, 2)
        W.entries.addWidget(W.entry8, 8, 3)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.continu, 9, 1)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    set_line_point_to_point(P, W)
