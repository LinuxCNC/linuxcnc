'''
conv_bolt.py

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
from plasmac import bolt_circle as BOLT

_translate = QCoreApplication.translate

def preview(P, W, Conv):
    if P.dialogError:
        return
    if not W.xsEntry.text():
        W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
    if not W.ysEntry.text():
        W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
    origin = W.centLeft.text() == 'CENTER'
    error = BOLT.preview(Conv, P.fTmp, P.fNgc, P.fNgcBkp, \
            int(W.conv_material.currentText().split(':')[0]), \
            W.conv_material.currentText().split(':')[1].strip(), \
            P.preAmble, P.postAmble, \
            W.liEntry.text(), W.loEntry.text(), W.aEntry.text(), \
            origin, W.xsEntry.text(), W.ysEntry.text(), \
            W.kerf_width.value(), \
            W.overcut.isChecked(), W.ocEntry.text(), \
            P.holeDiameter, P.holeSpeed, W.dEntry.text(), W.hdEntry.text(), \
            W.hEntry.text(), W.caEntry.text(), P.invalidLeads)
    if error:
        P.dialogError = True
        P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Bolt-Circle Error'), error)
    else:
        W.conv_preview.load(P.fNgc)
        W.conv_preview.set_current_view()
        W.add.setEnabled(True)
        W.undo.setEnabled(True)
        Conv.conv_preview_button(P, W, True)

def auto_preview(P, W, Conv, button=False):
    if button == 'center':
        if not W.centLeft.isChecked():
            return
        Conv.conv_auto_preview_button(P, W, button)
    if W.main_tab_widget.currentIndex() == 1 and \
       W.dEntry.text() and W.hdEntry.text() and W.hEntry.text():
        preview(P, W, Conv)

def entry_changed(P, W, Conv, widget):
    Conv.conv_entry_changed(P, W, widget, 'bolt')

def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(BOLT)
    W.dLabel.setText(_translate('Conversational', 'DIAMETER'))
    W.hLabel.setText(_translate('Conversational', '# OF HOLES'))
    W.loEntry.setText('0.0')
    W.hEntry.setObjectName('intEntry')
    W.lDesc.setText(_translate('Conversational', 'CREATING BOLT CIRCLE'))
    W.iLabel.setPixmap(P.conv_bolt_l)
    #alignment and size
    rightAlign = ['spLabel', 'xsLabel', 'xsEntry', 'ysLabel', 'ysEntry', \
                  'liLabel', 'liEntry', 'loLabel', 'loEntry', 'dLabel', 'dEntry', \
                  'hdLabel', 'hdEntry', 'hLabel', 'hEntry', \
                  'aLabel', 'aEntry', 'caLabel', 'caEntry', 'ocEntry']
    centerAlign = ['lDesc']
    leftAlign = ['ocLabel']
    rButton = ['centLeft']
    pButton = ['preview', 'add', 'undo', 'overcut']
    for widget in rightAlign:
        W[widget].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in centerAlign:
        W[widget].setAlignment(Qt.AlignCenter | Qt.AlignBottom)
        W[widget].setFixedWidth(240)
        W[widget].setFixedHeight(24)
    for widget in leftAlign:
        W[widget].setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in rButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in pButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    #connections
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W, Conv))
    W.centLeft.toggled.connect(lambda:auto_preview(P, W, Conv, 'center'))
    W.overcut.toggled.connect(lambda:auto_preview(P, W, Conv))
    W.preview.pressed.connect(lambda:preview(P, W, Conv))
    W.add.pressed.connect(lambda:Conv.conv_add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:Conv.conv_undo_shape(P, W))
    entries = ['ocEntry', 'xsEntry', 'ysEntry', 'liEntry', 'loEntry', \
               'dEntry', 'hdEntry', 'hEntry', 'aEntry', 'caEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, Conv, W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W, Conv))
    #add to layout
    if P.landscape:
        W.entries.addWidget(W.spLabel, 0, 0)
        W.entries.addWidget(W.centLeft, 0, 1)
        W.entries.addWidget(W.xsLabel, 1, 0)
        W.entries.addWidget(W.xsEntry, 1, 1)
        W.entries.addWidget(W.ysLabel, 2, 0)
        W.entries.addWidget(W.ysEntry, 2, 1)
        W.entries.addWidget(W.liLabel, 3, 0)
        W.entries.addWidget(W.liEntry, 3, 1)
        W.entries.addWidget(W.loLabel, 4, 0)
        W.entries.addWidget(W.loEntry, 4, 1)
        W.entries.addWidget(W.dLabel, 5, 0)
        W.entries.addWidget(W.dEntry, 5, 1)
        W.entries.addWidget(W.hdLabel, 6, 0)
        W.entries.addWidget(W.hdEntry, 6, 1)
        W.entries.addWidget(W.hLabel, 7, 0)
        W.entries.addWidget(W.hEntry, 7, 1)
        W.entries.addWidget(W.aLabel, 8, 0)
        W.entries.addWidget(W.aEntry, 8, 1)
        W.entries.addWidget(W.caLabel, 9, 0)
        W.entries.addWidget(W.caEntry, 9, 1)
        W.entries.addWidget(W.overcut, 10, 0)
        W.entries.addWidget(W.ocEntry, 10, 1)
        W.entries.addWidget(W.ocLabel, 10, 2)
        W.s11 = QLabel('')
        W.s11.setFixedHeight(24)
        W.entries.addWidget(W.s11, 11, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 2, 7, 3)
    else:
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.overcut, 1, 0)
        W.entries.addWidget(W.ocLabel, 1, 1)
        W.entries.addWidget(W.ocEntry, 1, 2)
        W.entries.addWidget(W.spLabel, 2, 0)
        W.entries.addWidget(W.centLeft, 2, 1)
        W.entries.addWidget(W.xsLabel, 3, 0)
        W.entries.addWidget(W.xsEntry, 3, 1)
        W.entries.addWidget(W.ysLabel, 3, 2)
        W.entries.addWidget(W.ysEntry, 3, 3)
        W.entries.addWidget(W.liLabel, 4, 0)
        W.entries.addWidget(W.liEntry, 4, 1)
        W.entries.addWidget(W.loLabel, 4, 2)
        W.entries.addWidget(W.loEntry, 4, 3)
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
    P.convSettingsChanged = False
