'''
conv_gusset.py

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
from plasmac import gusset as GUSSET

_translate = QCoreApplication.translate

def preview(P, W, Conv):
    if P.dialogError:
        return
    if not W.xsEntry.text():
        W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
    if not W.ysEntry.text():
        W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
    error = GUSSET.preview(Conv, P.fTmp, P.fNgc, P.fNgcBkp, \
            int(W.conv_material.currentText().split(':')[0]), \
            W.conv_material.currentText().split(':')[1].strip(), \
            P.preAmble, P.postAmble, \
            W.liEntry.text(), W.loEntry.text(), \
            W.xsEntry.text(), W.ysEntry.text(), \
            W.kerf_width.value(), P.intExt, \
            W.wEntry.text(), W.hEntry.text(), W.aEntry.text(), \
            W.rEntry.text(), W.rButton.text())
    if error:
        P.dialogError = True
        P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Gusset Error'), error)
    else:
        W.conv_preview.load(P.fNgc)
        W.conv_preview.set_current_view()
        W.add.setEnabled(True)
        W.undo.setEnabled(True)
        Conv.conv_preview_button(P, W, True)

def auto_preview(P, W, Conv, button=False):
    if button == 'intext':
        if not W.intExt.isChecked():
            return
        Conv.conv_auto_preview_button(P, W, button)
    elif button == 'center':
        if not W.centLeft.isChecked():
            return
        Conv.conv_auto_preview_button(P, W, button)
    if W.main_tab_widget.currentIndex() == 1 and \
       W.wEntry.text() and W.hEntry.text():
        preview(P, W, Conv)

def entry_changed(P, W, Conv, widget):
    Conv.conv_entry_changed(P, W, widget)

def rad_button_pressed(P, W, Conv, widget):
    if widget.text() == _translate('Conversational', 'RADIUS'):
        widget.setText(_translate('Conversational', 'CHAMFER'))
    else:
        widget.setText(_translate('Conversational', 'RADIUS'))
    auto_preview(P, W, Conv)

def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(GUSSET)
    W.rEntry.setText('0.0')
    W.hLabel.setText(_translate('Conversational', 'HEIGHT'))
    W.hEntry.setObjectName('')
    W.lDesc.setText(_translate('Conversational', 'CREATING GUSSET'))
    W.iLabel.setPixmap(P.conv_gusset_l)
    #alignment and size
    rightAlign = ['ctLabel', 'xsLabel', 'xsEntry', 'ysLabel', 'ysEntry', \
                  'liLabel', 'liEntry', 'loLabel', 'loEntry', 'wLabel', \
                  'wEntry', 'hLabel', 'hEntry', 'rEntry', 'aLabel', 'aEntry']
    centerAlign = ['lDesc']
    rButton = ['intExt']
    pButton = ['preview', 'add', 'undo', 'rButton']
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
    #connections
    # we need an exception handler here as there is no signal connected if it is the first instance
    try:
        W.preview.pressed.disconnect()
        W.undo.pressed.disconnect()
    except:
        pass
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W, Conv))
    W.intExt.toggled.connect(lambda:auto_preview(P, W, Conv, 'intext'))
    W.rButton.pressed.connect(lambda:rad_button_pressed(P, W, Conv, W.sender()))
    W.preview.pressed.connect(lambda:preview(P, W, Conv))
    W.add.pressed.connect(lambda:Conv.conv_add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda:Conv.conv_undo_shape(P, W))
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'wEntry', 'hEntry', 'rEntry', 'aEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, Conv, W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W, Conv))
    #add to layout
    if P.landscape:
        W.entries.addWidget(W.ctLabel, 0, 0)
        W.entries.addWidget(W.intExt, 0, 1)
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
        for r in [9,10,11]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 2, 7, 3)
    else:
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.ctLabel, 1, 0)
        W.entries.addWidget(W.intExt, 1, 1)
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
    P.convSettingsChanged = False
