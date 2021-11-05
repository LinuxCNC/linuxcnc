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
    invalidLine = False
    # check all entries are valid
#    try:
    if 1:
        try:
            if not W.entry1.text():
                W.entry1.setText('{:0.3f}'.format(P.xLineStart))
            P.xLineStart = float(W.entry1.text())
            W.entry1.setText('{:0.3f}'.format(float(W.entry1.text())))
        except:
            msg0 = _translate('Conversational', 'Invalid X ORIGIN entry detected')
            error_set(P, '{}.\n'.format(msg0))
            invalidLine = True
        try:
            if not W.entry2.text():
                W.entry2.setText('{:0.3f}'.format(P.yLineStart))
            P.yLineStart = float(W.entry2.text())
            W.entry2.setText('{:0.3f}'.format(float(W.entry2.text())))
        except:
            msg0 = _translate('Conversational', 'Invalid Y ORIGIN entry detected')
            error_set(P, '{}.\n'.format(msg0))
            invalidLine = True
        if P.lAlias == 'LP2P':
            if P.landscape:
                entry_check(P, W, W.entry4, 'x')
                entry_check(P, W, W.entry5, 'x')
                entry = [W.entry4, W.entry5]
            else:
                entry_check(P, W, W.entry3, 'x')
                entry_check(P, W, W.entry4, 'y')
                entry = [W.entry3, W.entry4]
            if float(entry[0].text()) == float(W.entry1.text()) and \
               float(entry[1].text()) == float(W.entry2.text()):
                msg0 = _translate('Conversational', 'Line length would be zero')
                error_set(P, '{}.\n'.format(msg0))
                invalidLine = True
            else:
                invalidLine = do_line_point_to_point(P, W, float(entry[0].text()), \
                                                           float(entry[1].text()))
        elif P.lAlias == 'LBLA':
            if P.landscape:
                entry_check(P, W, W.entry4, None)
                entry_check(P, W, W.entry5, None)
                entry = [W.entry4, W.entry5]
            else:
                entry_check(P, W, W.entry3, None)
                entry_check(P, W, W.entry5, None)
                entry = [W.entry3, W.entry5]
            if float(entry[0].text()) <= 0:
                msg0 = _translate('Conversational', 'Length must be greater than zero')
                error_set(P, '{}.\n'.format(msg0))
                invalidLine = True
            else:
                invalidLine = do_line_by_angle(P, W, float(entry[0].text()), \
                                                     float(entry[1].text()))
        elif P.lAlias == 'A3Pt':
            if P.landscape:
                entry_check(P, W, W.entry4, 'x')
                entry_check(P, W, W.entry5, 'y')
                entry_check(P, W, W.entry7, 'x')
                entry_check(P, W, W.entry8, 'y')
                entry = [W.entry4, W.entry5, W.entry7, W.entry8]
            else:
                entry_check(P, W, W.entry3, 'x')
                entry_check(P, W, W.entry4, 'y')
                entry_check(P, W, W.entry5, 'x')
                entry_check(P, W, W.entry6, 'y')
                entry = [W.entry3, W.entry4, W.entry5, W.entry6]
            invalidLine = do_arc_3_points(P, W, float(entry[0].text()), \
                                                float(entry[1].text()),
                                                float(entry[2].text()),
                                                float(entry[3].text()))
        elif P.lAlias == 'A2PR':
            if P.landscape:
                entry_check(P, W, W.entry4, 'x')
                entry_check(P, W, W.entry5, 'y')
                entry_check(P, W, W.entry7, None)
                entry = [W.entry4, W.entry5, W.entry7]
            else:
                entry_check(P, W, W.entry3, 'x')
                entry_check(P, W, W.entry4, 'y')
                entry_check(P, W, W.entry5, None)
                entry = [W.entry3, W.entry4, W.entry5]
            if float(entry[2].text()) == 0:
                msg0 = _translate('Conversational', 'Radius must be greater than zero')
                error_set(P, '{}.\n'.format(msg0))
                invalidLine = True
            else:
                invalidLine = do_arc_2_points_radius(P, W, float(entry[0].text()), \
                                                           float(entry[1].text()),
                                                           float(entry[2].text()))
        elif P.lAlias == 'ALAR':
            if P.landscape:
                entry_check(P, W, W.entry4, None)
                entry_check(P, W, W.entry5, None)
                entry_check(P, W, W.entry7, None)
                entry = [W.entry4, W.entry5, W.entry7]
                radius = float(W.entry7.text())
            else:
                entry_check(P, W, W.entry3, None)
                entry_check(P, W, W.entry4, None)
                entry_check(P, W, W.entry5, None)
                entry = [W.entry3, W.entry4, W.entry5]
                radius = float(W.entry5.text())
            if float(entry[2].text()) == 0:
                msg0 = _translate('Conversational', 'Radius must be greater than zero')
                error_set(P, '{}.\n'.format(msg0))
                invalidLine = True
            else:
                invalidLine = do_arc_by_angle_radius(P, W, float(entry[0].text()), \
                                                           float(entry[1].text()),
                                                           float(entry[2].text()))
#    except:
    else:
        msg0 = _translate('Conversational', 'Invalid entry detected')
        error_set(P, '{}.\n'.format(msg0))
        return
    if invalidLine:
        return
    #do next segment
    if P.conv_add_segment == 1:
        inTmp = open(P.fNgc, 'r')
        outTmp = open(P.fTmp, 'w')
        while(1):
            line = inTmp.readline()
            if not line or 'M5 $0' in line:
                break
            else:
                outTmp.write(line)
        inTmp.close()
        COPY(P.fTmp, P.fNgc)
        outNgc = open(P.fNgc, 'w')
    # do first segment
    else:
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
        outTmp.write('\n(conversational {})\n'.format(W.lType.currentText()))
        outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(P.xLineStart, P.yLineStart))
        outTmp.write('m3 $0 s1\n')
    # write the gcode
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
    P.conv_preview_button(True)
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(True)
    W.undo.setEnabled(True)
    if P.conv_add_segment == 1:
        P.conv_add_segment = 2
    P.previewActive = True

def entry_check(P, W, widget, coord):
    if coord:
        if widget.text():
            widget.setText('{:0.3f}'.format(float(widget.text())))
        else:
            widget.setText('{:0.3f}'.format(P['{}LineStart'.format(coord)]))
    else:
        if widget.text():
            widget.setText('{:0.3f}'.format(float(widget.text())))
        else:
            widget.setText('0.000')

def error_set(P, error):
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Line Error'), error)

def do_line_point_to_point(P, W, inX, inY):
    P.xLineEnd = inX
    P.yLineEnd = inY
    P.conv_gcodeLine = 'g1 x{:.6f} y{:.6f}\n'.format(float(P.xLineEnd), float(P.yLineEnd))
    return(False)

def do_line_by_angle(P, W, inL, inA):
    angle = math.radians(inA)
    P.xLineEnd = P.xLineStart + (inL * math.cos(angle))
    P.yLineEnd = P.yLineStart + (inL * math.sin(angle))
    P.conv_gcodeLine = 'g1 x{:.6f} y{:.6f}\n'.format(P.xLineEnd, P.yLineEnd)
    return(False)

def do_arc_3_points(P, W, inX1, inY1, inXE, inYE):
    P.xLineEnd = inXE
    P.yLineEnd = inYE
    A = numpy.array([P.xLineStart, P.yLineStart, 0.0])
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
    G = '3' if (inX1-P.xLineStart)*(P.yLineEnd-P.yLineStart)-(inY1-P.yLineStart)*(P.xLineEnd-P.xLineStart) > 0 else '2'
    if numpy.isnan(p[0] - P.xLineStart) or numpy.isnan(p[1] - P.yLineStart):
        msg0 = _translate('Conversational', 'Unknown calculation error')
        msg1 = _translate('Conversational', 'Ensure entries are correct')
        error_set(P, '{},\n\n{}.\n'.format(msg0, msg1))
        return(True)
    else:
        P.conv_gcodeLine = 'g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(G, P.xLineEnd, P.yLineEnd, p[0] - P.xLineStart, p[1] - P.yLineStart)
    return(False)

def do_arc_2_points_radius(P, W, inXE, inYE, radius):
    P.xLineEnd = inXE
    P.yLineEnd = inYE
    try:
        dir = math.radians(270) if radius > 0 else math.radians(90)
        arcType = '2' if W.g2Arc.isChecked() else '3'
        height = math.sqrt((P.xLineEnd - P.xLineStart) ** 2 + (P.yLineEnd - P.yLineStart) ** 2) * 0.5
        length = math.sqrt((radius ** 2) - (height ** 2))
        angle = math.atan2((P.yLineEnd - P.yLineStart), (P.xLineEnd - P.xLineStart))
        xLineCentre = (P.xLineStart + P.xLineEnd) / 2
        yLineCentre = (P.yLineStart + P.yLineEnd) / 2
        xArcCentre = xLineCentre + length * math.cos(angle + dir)
        yArcCentre = yLineCentre + length * math.sin(angle + dir)
    except:
        msg0 = _translate('Conversational', 'Arc calculation error')
        msg1 = _translate('Conversational', 'Try a larger radius')
        error_set(P, '{},\n\n{}.\n'.format(msg0, msg1))
        return(True)
    P.conv_gcodeLine = ('g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(arcType, P.xLineEnd, P.yLineEnd, xArcCentre - P.xLineStart, yArcCentre - P.yLineStart))
    return(False)

def do_arc_by_angle_radius(P, W, inL, inA, inR):
    angle = math.radians(inA)
    xE = P.xLineStart + (inL * math.cos(angle))
    yE = P.yLineStart + (inL * math.sin(angle))
    result = do_arc_2_points_radius(P, W, xE, yE, inR)
    return(result)

def set_start_point(P, W, pixmap):
    W.iLabel.setPixmap(pixmap)
    text = _translate('Conversational', 'START')
    W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
    W.entry1.setText('{:0.3f}'.format(P.xLineStart))
    W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
    W.entry2.setText('{:0.3f}'.format(P.yLineStart))

def set_line_point_to_point(P, W, refresh):
    if P.landscape:
        W.entry4.setFocus()
    else:
        W.entry3.setFocus()
    if refresh:
        return
    set_start_point(P, W, P.pixLinePoint)
    text = _translate('Conversational', 'END')
    for n in ['3', '4', '5', '6', '7', '8']:
        W['entry{}'.format(n)].setText('')
        W['label{}'.format(n)].setText('')
        W['entry{}'.format(n)].hide()
#        W['label{}'.format(n)].hide()
    W.g3Arc.hide()
    W.g2Arc.hide()
    if P.landscape:
        W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.label4.show()
        W.entry4.show()
        W.label5.show()
        W.entry5.show()
    else:
        W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.label3.show()
        W.entry3.show()
        W.label4.show()
        W.entry4.show()

def set_line_by_angle(P, W, refresh):
    if P.landscape:
        W.entry4.setFocus()
    else:
        W.entry3.setFocus()
    if refresh:
        return
    set_start_point(P, W, P.pixLineAngle)
    for n in ['3', '4', '5', '6', '7', '8']:
        W['entry{}'.format(n)].setText('')
        W['label{}'.format(n)].setText('')
        W['entry{}'.format(n)].hide()
#        W['label{}'.format(n)].hide()
    W.g3Arc.hide()
    W.g2Arc.hide()
    if P.landscape:
        W.label4.setText(_translate('Conversational', 'LENGTH'))
        W.label5.setText(_translate('Conversational', 'ANGLE'))
        W.entry5.setText('0.000')
        W.label4.show()
        W.entry4.show()
        W.label5.show()
        W.entry5.show()
    else:
        W.label3.setText(_translate('Conversational', 'LENGTH'))
        W.label5.setText(_translate('Conversational', 'ANGLE'))
        W.entry5.setText('0.000')
        W.label3.show()
        W.entry3.show()
        W.label5.show()
        W.entry5.show()

def set_arc_3_points(P, W, refresh):
    if P.landscape:
        W.entry4.setFocus()
    else:
        W.entry3.setFocus()
    if refresh:
        return
    set_start_point(P, W, P.pixArc3p)
    text = _translate('Conversational', 'NEXT')
    for n in ['3', '4', '5', '6', '7', '8']:
        W['entry{}'.format(n)].setText('')
        W['label{}'.format(n)].setText('')
        W['entry{}'.format(n)].hide()
#        W['label{}'.format(n)].hide()
    for n in ['4', '5']:
        W['entry{}'.format(n)].show()
#        W['label{}'.format(n)].show()
    W.g3Arc.hide()
    W.g2Arc.hide()
    if P.landscape:
        W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
        text = _translate('Conversational', 'END')
        W.label7.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label8.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.label7.show()
        W.entry7.show()
        W.label8.show()
        W.entry8.show()
    else:
        W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
        text = _translate('Conversational', 'END')
        W.label5.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label6.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.label3.show()
        W.entry3.show()
        W.label6.show()
        W.entry6.show()

def set_arc_2_points_radius(P, W, refresh):
    if P.landscape:
        W.entry4.setFocus()
    else:
        W.entry3.setFocus()
    if refresh:
        return
    set_start_point(P, W, P.pixArc2pr)
    text = _translate('Conversational', 'END')
    for n in ['3', '4', '5', '6', '7', '8']:
        W['entry{}'.format(n)].setText('')
        W['label{}'.format(n)].setText('')
        W['entry{}'.format(n)].hide()
#        W['label{}'.format(n)].hide()
    for n in ['4', '5']:
        W['entry{}'.format(n)].show()
#        W['label{}'.format(n)].show()
    set_arc_widgets(P, W)
    if P.landscape:
        W.label4.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label5.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.label7.setText(_translate('Conversational', 'RADIUS'))
        W.entry7.setText('0.000')
        W.label7.show()
        W.entry7.show()
    else:
        W.label3.setText(_translate('Conversational', 'X {}'.format(text)))
        W.label4.setText(_translate('Conversational', 'Y {}'.format(text)))
        W.label5.setText(_translate('Conversational', 'RADIUS'))
        W.entry5.setText('0.000')
        W.label3.show()
        W.entry3.show()

def set_arc_by_angle_radius(P, W, refresh):
    if P.landscape:
        W.entry4.setFocus()
    else:
        W.entry3.setFocus()
    if refresh:
        return
    set_start_point(P, W, P.pixArcAngle)
    for n in ['3', '4', '5', '6', '7', '8']:
        W['entry{}'.format(n)].setText('')
        W['label{}'.format(n)].setText('')
        W['entry{}'.format(n)].hide()
#        W['label{}'.format(n)].hide()
    for n in ['4', '5']:
        W['entry{}'.format(n)].show()
#        W['label{}'.format(n)].show()
    set_arc_widgets(P, W)
    if P.landscape:
        W.label4.setText(_translate('Conversational', 'LENGTH'))
        W.label5.setText(_translate('Conversational', 'ANGLE'))
        W.entry5.setText('0.000')
        W.label7.setText(_translate('Conversational', 'RADIUS'))
        W.label7.show()
        W.entry7.show()
    else:
        W.label3.setText(_translate('Conversational', 'LENGTH'))
        W.label4.setText(_translate('Conversational', 'ANGLE'))
        W.entry4.setText('0.000')
        W.label5.setText(_translate('Conversational', 'RADIUS'))
        W.label3.show()
        W.entry3.show()

def set_arc_widgets(P, W):
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

def line_type_changed(P, W, refresh):
    if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
        P.lAlias = 'LP2P'
        set_line_point_to_point(P, W, refresh)
    elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
        P.lAlias = 'LBLA'
        set_line_by_angle(P, W, refresh)
    elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
        P.lAlias = 'A3Pt'
        set_arc_3_points(P, W, refresh)
    elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
        P.lAlias = 'A2PR'
        set_arc_2_points_radius(P, W, refresh)
    elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
        P.lAlias = 'ALAR'
        set_arc_by_angle_radius(P, W, refresh)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and W.entry1.text() and W.entry2.text():
        if P.landscape:
            if (P.lAlias == 'LP2P' and W.entry4.text() and W.entry5.text()) or \
               (P.lAlias == 'LBLA' and W.entry4.text() and W.entry5.text()) or \
               (P.lAlias == 'A3Pt' and W.entry4.text() and W.entry5.text() and W.entry7.text() and W.entry8.text()) or \
               (P.lAlias == 'A2PR' and W.entry4.text() and W.entry5.text() and W.entry7.text()) or \
               (P.lAlias == 'ALAR' and W.entry4.text() and W.entry5.text() and W.entry7.text()):
                preview(P, W)
        else:
            if (P.lAlias == 'LP2P' and W.entry3.text() and W.entry4.text()) or \
               (P.lAlias == 'LBLA' and W.entry3.text() and W.entry5.text()) or \
               (P.lAlias == 'A3Pt' and W.entry3.text() and W.entry4.text() and W.entry5.text() and W.entry6.text()) or \
               (P.lAlias == 'A2PR' and W.entry3.text() and W.entry4.text() and W.entry5.text()) or \
               (P.lAlias == 'ALAR' and W.entry3.text() and W.entry4.text() and W.entry5.text()):
                preview(P, W)

def add_shape_to_file(P, W):
    P.conv_gcodeSave = P.conv_gcodeLine
    P.conv_add_shape_to_file()
    P.xLineStart = P.xLineEnd
    P.yLineStart = P.yLineEnd
    W.entry1.setText('{:0.3f}'.format(P.xLineEnd))
    W.entry2.setText('{:0.3f}'.format(P.yLineEnd))
    P.conv_add_segment = 1
    line_type_changed(P, W, True)
    W.add.setEnabled(False)
    P.previewActive = False

def undo_shape(P, W):
    cancel = P.conv_undo_shape()
    if cancel:
        return
    if P.previewActive:
        if P.conv_add_segment > 1:
            P.conv_add_segment = 1
        line_type_changed(P, W, True) # undo
    else:
        P.conv_add_segment = 0
        P.xLineStart = 0.000
        P.yLineStart = 0.000
        line_type_changed(P, W, False)
    P.xLineEnd = P.xLineStart
    P.yLineEnd = P.yLineStart
    if len(P.conv_gcodeSave):
        P.conv_gcodeLine = P.conv_gcodeSave
    P.previewActive = False

def entry_changed(P, W, widget, entry):
    if (P.lAlias == 'LBLA' and P.landscape and widget == W.entry4) or \
       (P.lAlias == 'LBLA' and not P.landscape and widget == W.entry3) or \
       (P.lAlias == 'ALAR' and P.landscape and widget == W.entry4) or \
       (P.lAlias == 'ALAR' and not P.landscape and widget == W.entry4):
        widget.setObjectName(None)
    else:
        widget.setObjectName('aEntry')
    P.conv_entry_changed(widget)

def widgets(P, W):
    P.lAlias = 'LP2P'
    P.previewActive = False
    if not P.convSettingsChanged:
        #widgets
        W.lType = QComboBox()
        W.label1 = QLabel()
#        W.entry1 = QLineEdit(P.xSaved)
        W.entry1 = QLineEdit()
        W.label2 = QLabel()
#        W.entry2 = QLineEdit(P.ySaved)
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
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING LINE OR ARC'))
    text = _translate('Conversational', 'CW')
    W.g2Arc = QRadioButton('      {}'.format(text))
    text = _translate('Conversational', 'CCW')
    W.g3Arc = QRadioButton('     {}'.format(text))
    W.iLabel = QLabel()
    P.pixLinePoint = QPixmap('{}conv_line_point.png'.format(P.IMAGES)).scaledToWidth(196)
    P.pixLineAngle = QPixmap('{}conv_line_angle.png'.format(P.IMAGES)).scaledToWidth(196)
    P.pixArc3p = QPixmap('{}conv_arc_3p.png'.format(P.IMAGES)).scaledToWidth(196)
    P.pixArc2pr = QPixmap('{}conv_arc_2pr.png'.format(P.IMAGES)).scaledToWidth(196)
    P.pixArcAngle = QPixmap('{}conv_arc_angle.png'.format(P.IMAGES)).scaledToWidth(196)
    #alignment and size
    rightAlign = ['label1', 'entry1', 'label2', 'entry2', 'label3', 'entry3', \
                  'label4', 'entry4', 'label5', 'entry5', 'label6', 'entry6', \
                  'label7', 'entry7', 'label8', 'entry8']
    centerAlign = ['lDesc']
    rButton = ['g2Arc', 'g3Arc']
    pButton = ['preview', 'add', 'undo']
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
    W.lType.addItem(_translate('Conversational', 'LINE POINT ~ POINT'))
    W.lType.addItem(_translate('Conversational', 'LINE BY ANGLE'))
    W.lType.addItem(_translate('Conversational', 'ARC 3P'))
    W.lType.addItem(_translate('Conversational', 'ARC 2P +RADIUS'))
    W.lType.addItem(_translate('Conversational', 'ARC ANGLE +RADIUS'))
    P.conv_add_segment = 0
    P.conv_gcodeLine = ''
    P.conv_gcodeSave = ''
    P.xLineStart = 0.000
    P.yLineStart = 0.000
    if not W.g2Arc.isChecked() and not W.g3Arc.isChecked():
        W.g2Arc.setChecked(True)
    #connections
    # we need an exception handler here as there is no signal connected in the first instance
    try:
        W.preview.pressed.disconnect()
        W.undo.pressed.disconnect()
    except:
        pass
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:undo_shape(P, W))
    W.lType.currentTextChanged.connect(lambda:line_type_changed(P, W, False))
    W.g2Arc.toggled.connect(lambda:auto_preview(P, W))
    entries = ['entry1', 'entry2', 'entry3', 'entry4',
               'entry5', 'entry6', 'entry7', 'entry8']
    for entry in entries:
        W[entry].textChanged.connect(lambda w:entry_changed(P, W, W.sender(), w))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
#        for row in range (14):
#            W.entries.setRowMinimumHeight(row, 24)
        W.entries.addWidget(W.lType, 0, 0, 1, 2)
        W.lType.setFixedHeight(24)
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
        for row in range (11):
            W.entries.setRowMinimumHeight(row, 24)
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.lType, 1, 0, 1, 2)
        W.entries.addWidget(W.label1, 2, 0)
        W.entries.addWidget(W.entry1, 2, 1)
        W.entries.addWidget(W.label2, 2, 2)
        W.entries.addWidget(W.entry2, 2, 3)
        W.entries.addWidget(W.s3, 3, 0)
        W.entries.addWidget(W.label3, 4, 0)
        W.entries.addWidget(W.entry3, 4, 1)
        W.entries.addWidget(W.label4, 4, 2)
        W.entries.addWidget(W.entry4, 4, 3)
        W.entries.addWidget(W.s5, 5, 0)
        W.entries.addWidget(W.label5, 6, 0)
        W.entries.addWidget(W.entry5, 6, 1)
        W.entries.addWidget(W.label6, 6, 2)
        W.entries.addWidget(W.entry6, 6, 3)
        for r in [3,5,7]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.label7, 8, 0)
        W.entries.addWidget(W.entry7, 8, 1)
        W.entries.addWidget(W.label8, 8, 2)
        W.entries.addWidget(W.entry8, 8, 3)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    if not P.convSettingsChanged:
        line_type_changed(P, W, False)
    P.convSettingsChanged = False
