'''
conv_line.py

Copyright (C) 2020, 2021, 2022  Phillip A Carter
Copyright (C) 2020, 2021, 2022  Gregory D Carl

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

from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QMessageBox
from importlib import reload
from plasmac import line as LINE

_translate = QCoreApplication.translate

def preview(P, W, Conv):
    if P.xLineEnd != Conv.conv_is_float(W.entry1.text())[1] or \
       P.yLineEnd != Conv.conv_is_float(W.entry2.text())[1]:
        P.convAddSegment = 0
    if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
        reply = LINE.do_line_point_to_point(Conv, W.entry1.text(), W.entry2.text(), W.entry3.text(), \
                                                  W.entry4.text())
        if not reply[0]:
            P.xLineEnd = reply[1]
            P.yLineEnd = reply[2]
            P.conv_gcodeLine = reply[3]
        else:
            error_set(P, '{}\n'.format(reply[1]))
            return
    elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
        reply = LINE.do_line_by_angle(Conv, W.entry1.text(), W.entry2.text(), W.entry3.text(), \
                                            W.entry4.text())
        if not reply[0]:
            P.xLineEnd = reply[1]
            P.yLineEnd = reply[2]
            P.conv_gcodeLine = reply[3]
        else:
            error_set(P, '{}\n'.format(reply[1]))
            return
    elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
        reply = LINE.do_arc_3_points(Conv, W.entry1.text(), W.entry2.text(), W.entry3.text(), \
                                           W.entry4.text(), W.entry5.text(), W.entry6.text())
        if not reply[0]:
            P.xLineEnd = reply[1]
            P.yLineEnd = reply[2]
            P.conv_gcodeLine = reply[3]
        else:
            error_set(P, '{}\n'.format(reply[1]))
            return
    elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
        arcType = '3' if 'CCW' in W.g23Arc.text() else '2'
        reply = LINE.do_arc_2_points_radius(Conv, W.entry1.text(), W.entry2.text(), W.entry3.text(), \
                                                  W.entry4.text(), W.entry5.text(), arcType)
        if not reply[0]:
            P.xLineEnd = reply[1]
            P.yLineEnd = reply[2]
            P.conv_gcodeLine = reply[3]
        else:
            error_set(P, '{}\n'.format(reply[1]))
            return
    elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
        arcType = '3' if 'CCW' in W.g23Arc.text() else '2'
        reply = LINE.do_arc_by_angle_radius(Conv, W.entry1.text(), W.entry2.text(), W.entry3.text(),
                                                  W.entry4.text(), W.entry5.text(), arcType)
        if not reply[0]:
            P.xLineEnd = reply[1]
            P.yLineEnd = reply[2]
            P.conv_gcodeLine = reply[3]
        else:
            error_set(P, '{}\n'.format(reply[1]))
            return
    if P.convAddSegment == 1:
        LINE.next_segment(P.fTmp, P.fNgc)
    else:
        valid, P.xLineStart = Conv.conv_is_float(W.entry1.text())
        valid, P.yLineStart = Conv.conv_is_float(W.entry2.text())
        LINE.first_segment(P.fTmp, P.fNgc, P.fNgcBkp, P.preAmble, \
                           W.lType.currentText(), P.xLineStart, P.yLineStart, \
                           int(W.conv_material.currentText().split(':')[0]), \
                           W.conv_material.currentText().split(':')[1].strip())
    LINE.last_segment(P.fTmp, P.fNgc, P.conv_gcodeLine, P.postAmble)
    Conv.conv_preview_button(P, W, True)
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(True)
    W.undo.setEnabled(True)
    if P.convAddSegment == 1:
        P.convAddSegment = 2
    P.previewActive = True

def auto_preview(P, W, Conv):
    # only act if checked as we uncheck it here
    if W.g23Arc.isChecked():
        text = 'CW - G2' if 'CCW' in W.g23Arc.text() else 'CCW - G3'
        W.g23Arc.setChecked(False)
        W.g23Arc.setText(text)
        if W.main_tab_widget.currentIndex() == 1 and \
           W.entry1.text() and W.entry2.text() and W.entry3.text() and W.entry4.text():
            if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT') or \
               W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE') or \
              (W.lType.currentText() == _translate('Conversational', 'ARC 3P') and W.entry5.text() and W.entry6.text()) or \
              (W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS') and W.entry5.text()) or \
              (W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS') and W.entry5.text()):
                preview(P, W, Conv)

def line_type_changed(P, W, refresh):
    W.entry3.setFocus()
    if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
        if not refresh:
            set_line_point_to_point(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
        if not refresh:
            set_line_by_angle(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
        if not refresh:
            set_arc_3_points(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
        if not refresh:
            set_arc_2_points_radius(P, W)
    elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
        if not refresh:
            set_arc_by_angle_radius(P, W)

def add_shape_to_file(P, W, Conv):
    P.conv_gcodeSave = P.conv_gcodeLine
    Conv.conv_add_shape_to_file(P, W)
    P.xLineStart = P.xLineEnd
    P.yLineStart = P.yLineEnd
    W.entry1.setText('{:0.3f}'.format(P.xLineEnd))
    W.entry2.setText('{:0.3f}'.format(P.yLineEnd))
    P.convAddSegment = 1
    line_type_changed(P, W, True)
    W.add.setEnabled(False)
    P.previewActive = False

def undo_shape(P, W, Conv):
    cancel = Conv.conv_undo_shape(P, W)
    if cancel:
        return
    if P.previewActive:
        if P.convAddSegment > 1:
            P.convAddSegment = 1
        line_type_changed(P, W, True) # undo
    else:
        P.convAddSegment = 0
        P.xLineStart = 0.000
        P.yLineStart = 0.000
        line_type_changed(P, W, False)
    P.xLineEnd = P.xLineStart
    P.yLineEnd = P.yLineStart
    if len(P.conv_gcodeSave):
        P.conv_gcodeLine = P.conv_gcodeSave
    P.previewActive = False

def clear_widgets(P, W, image):
    set_start_point(P, W, image)
    for n in '3456':
        W['entry{}'.format(n)].hide()
        W['label{}'.format(n)].setText('')
    W.g23Arc.hide()

def set_start_point(P, W, image):
    W.iLabel.setPixmap(image)
    text = _translate('Conversational', 'START')
    W.label1.setText(_translate('Conversational', 'X {}'.format(text)))
    W.entry1.setText('{:0.3f}'.format(P.xLineStart))
    W.label2.setText(_translate('Conversational', 'Y {}'.format(text)))
    W.entry2.setText('{:0.3f}'.format(P.yLineStart))

def set_line_point_to_point(P, W):
    clear_widgets(P, W, P.conv_line_point)
    W.label3.setText(_translate('Conversational', 'X END'))
    W.label4.setText(_translate('Conversational', 'Y END'))
    for n in '34':
        W['entry{}'.format(n)].setText('')
        W['entry{}'.format(n)].show()
    W.entry3.setObjectName('neg')

def set_line_by_angle(P, W):
    clear_widgets(P, W, P.conv_line_angle)
    W.label3.setText(_translate('Conversational', 'LENGTH'))
    W.label4.setText(_translate('Conversational', 'ANGLE'))
    W.entry4.setText('0.000')
    for n in '34':
        W['entry{}'.format(n)].setText('')
        W['entry{}'.format(n)].show()
    W.entry3.setObjectName(None)

def set_arc_3_points(P, W):
    clear_widgets(P, W, P.conv_line_3p)
    W.label3.setText(_translate('Conversational', 'X NEXT'))
    W.label4.setText(_translate('Conversational', 'Y NEXT'))
    W.label5.setText(_translate('Conversational', 'X END'))
    W.label6.setText(_translate('Conversational', 'Y END'))
    for n in '3456':
        W['entry{}'.format(n)].setText('')
        W['entry{}'.format(n)].show()
        W['entry{}'.format(n)].setObjectName('neg')

def set_arc_2_points_radius(P, W):
    clear_widgets(P, W, P.conv_line_2pr)
    set_arc_widgets(P, W)
    W.label3.setText(_translate('Conversational', 'X END'))
    W.label4.setText(_translate('Conversational', 'Y END'))
    W.label5.setText(_translate('Conversational', 'RADIUS'))
    W.entry5.setText('0.000')
    W.label6.setText(_translate('Conversational', 'DIRECTION'))
    for n in '345':
        W['entry{}'.format(n)].setText('')
        W['entry{}'.format(n)].show()
    W.entry3.setObjectName('neg')
    W.entry5.setObjectName(None)

def set_arc_by_angle_radius(P, W):
    clear_widgets(P, W, P.conv_arc_angle)
    set_arc_widgets(P, W)
    W.label3.setText(_translate('Conversational', 'LENGTH'))
    W.label4.setText(_translate('Conversational', 'ANGLE'))
    W.entry4.setText('0.000')
    W.label5.setText(_translate('Conversational', 'RADIUS'))
    W.label6.setText(_translate('Conversational', 'DIRECTION'))
    for n in '345':
        W['entry{}'.format(n)].setText('')
        W['entry{}'.format(n)].show()
    W.entry3.setObjectName(None)
    W.entry5.setObjectName(None)

def set_arc_widgets(P, W):
    W.entries.removeWidget(W.g23Arc)
    if P.landscape:
        W.entries.addWidget(W.g23Arc, 8, 1)
    else:
        W.entries.addWidget(W.g23Arc, 6, 3)
    W.label6.setText(_translate('Conversational', 'DIRECTION'))
    W.label6.show()
    W.g23Arc.show()

def error_set(P, error):
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Line Error'), error)

def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(LINE)
    P.previewActive = False
    W.lDesc.setText(_translate('Conversational', 'CREATING LINE OR ARC'))
    #alignment and size
    rightAlign = ['label1', 'entry1', 'label2', 'entry2', 'label3', 'entry3', \
                  'label4', 'entry4', 'label5', 'entry5', 'label6', 'entry6']
    centerAlign = ['lDesc']
    rButton = ['g23Arc']
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
    P.convAddSegment = 0
    P.conv_gcodeLine = ''
    P.conv_gcodeSave = ''
    P.xLineStart = P.xLineEnd = 0.000
    P.yLineStart = P.yLineEnd = 0.000
    #connections
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W, Conv))
    W.preview.pressed.connect(lambda:preview(P, W, Conv))
    W.add.pressed.connect(lambda:add_shape_to_file(P, W, Conv))
    W.undo.pressed.connect(lambda:undo_shape(P, W, Conv))
    W.lType.currentTextChanged.connect(lambda:line_type_changed(P, W, False))
    W.g23Arc.toggled.connect(lambda:auto_preview(P, W, Conv))
    entries = ['entry1', 'entry2', 'entry3', 'entry4', 'entry5', 'entry6']
    for entry in entries:
        W[entry].textChanged.connect(lambda w:Conv.conv_entry_changed(P, W, W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W, Conv))
    #add to layout
    if P.landscape:
        for row in range (14):
            W.entries.setRowMinimumHeight(row, 24)
        W.entries.addWidget(W.lType, 0, 0, 1, 2)
        W.lType.setFixedHeight(24)
        W.entries.addWidget(W.label1, 1, 0)
        W.entries.addWidget(W.entry1, 1, 1)
        W.entries.addWidget(W.label2, 2, 0)
        W.entries.addWidget(W.entry2, 2, 1)
        W.entries.addWidget(W.label3, 4, 0)
        W.entries.addWidget(W.entry3, 4, 1)
        W.entries.addWidget(W.label4, 5, 0)
        W.entries.addWidget(W.entry4, 5, 1)
        W.entries.addWidget(W.label5, 7, 0)
        W.entries.addWidget(W.entry5, 7, 1)
        W.entries.addWidget(W.label6, 8, 0)
        W.entries.addWidget(W.entry6, 8, 1)
        for r in [3,6,9,10,11]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 2, 7, 3)
    else:
        for row in range (11):
            W.entries.setRowMinimumHeight(row, 24)
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.lType, 1, 0, 1, 2)
        W.entries.addWidget(W.label1, 2, 0)
        W.entries.addWidget(W.entry1, 2, 1)
        W.entries.addWidget(W.label2, 2, 2)
        W.entries.addWidget(W.entry2, 2, 3)
        W.entries.addWidget(W.label3, 4, 0)
        W.entries.addWidget(W.entry3, 4, 1)
        W.entries.addWidget(W.label4, 4, 2)
        W.entries.addWidget(W.entry4, 4, 3)
        W.entries.addWidget(W.label5, 6, 0)
        W.entries.addWidget(W.entry5, 6, 1)
        W.entries.addWidget(W.label6, 6, 2)
        W.entries.addWidget(W.entry6, 6, 3)
        for r in [3,5,7,8]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    if not P.convSettingsChanged:
        line_type_changed(P, W, False)
    P.convSettingsChanged = False
