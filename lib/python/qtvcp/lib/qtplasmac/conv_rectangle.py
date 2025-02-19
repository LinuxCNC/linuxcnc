'''
conv_rectangle.py

Copyright (C) 2020 - 2024 Phillip A Carter
Copyright (C) 2020 - 2024 Gregory D Carl

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
from plasmac import rectangle as RECTANGLE

_translate = QCoreApplication.translate


def preview(P, W, Conv):
    if P.dialogError:
        return
    if not W.xsEntry.text():
        W.xsEntry.setText(f'{P.xOrigin:0.3f}')
    if not W.ysEntry.text():
        W.ysEntry.setText(f'{P.yOrigin:0.3f}')
    origin = W.centLeft.text() == 'CENTER'
    styles = [None, 'extRadius', 'extRadius', 'extRadius', 'extRadius']
    for n in range(1, 5):
        if W[f'r{n}Button'].text().startswith(_translate('Conversational', 'CHAMFER')):
            styles[n] = 'chamfer'
        elif W[f'r{n}Button'].text().startswith(_translate('Conversational', 'iRADIUS')):
            styles[n] = 'intRadius'
    error = RECTANGLE.preview(Conv, P.fTmp, P.fNgc, P.fNgcBkp,
                              int(W.conv_material.currentText().split(':')[0]),
                              W.conv_material.currentText().split(':')[1].strip(),
                              P.preAmble, P.postAmble,
                              W.liEntry.text(), W.loEntry.text(),
                              origin, W.xsEntry.text(), W.ysEntry.text(),
                              W.kerf_width.value(), P.intExt,
                              W.wEntry.text(), W.hEntry.text(), W.aEntry.text(),
                              styles[1], styles[2], styles[3], styles[4],
                              W.r1Entry.text(), W.r2Entry.text(), W.r3Entry.text(), W.r4Entry.text(),
                              W.r1Button.text(), W.r2Button.text(), W.r3Button.text(), W.r4Button.text())
    if error:
        P.dialogError = True
        P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Rectangle Error'), error)
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
#    if W.main_tab_widget.currentIndex() == 1 and \
#       W.wEntry.text() and W.hEntry.text():
    if W.main_tab_widget.currentIndex() == 1:
        preview(P, W, Conv)


def entry_changed(P, W, Conv, widget):
    Conv.conv_entry_changed(P, W, widget)


def rad_button_pressed(P, W, Conv, button, value):
    if button.text().split()[0] == _translate('Conversational', 'RADIUS'):
        text = _translate('Conversational', 'CHAMFER')
        button.setText(f'{text} {value}')
    elif button.text().split()[0] == _translate('Conversational', 'CHAMFER'):
        text = _translate('Conversational', 'iRADIUS')
        button.setText(f'{text} {value}')
    else:
        text = _translate('Conversational', 'RADIUS')
        button.setText(f'{text} {value}')
    auto_preview(P, W, Conv)


def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(RECTANGLE)
    W.hLabel.setText(_translate('Conversational', 'HEIGHT'))
    W.hEntry.setObjectName('')
    W.lDesc.setText(_translate('Conversational', 'CREATING RECTANGLE'))
    W.iLabel.setPixmap(P.conv_rectangle_l)
    # alignment and size
    rightAlign = ['ctLabel', 'spLabel', 'xsLabel', 'xsEntry', 'ysLabel',
                  'ysEntry', 'liLabel', 'liEntry', 'loLabel', 'loEntry',
                  'wLabel', 'wEntry', 'hLabel', 'hEntry', 'aLabel',
                  'aEntry', 'r1Entry', 'r2Entry', 'r3Entry', 'r4Entry']
    centerAlign = ['lDesc']
    rButton = ['intExt', 'centLeft']
    pButton = ['preview', 'add', 'undo',
               'r1Button', 'r2Button', 'r3Button', 'r4Button']
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
    # connections
    W.conv_material.currentTextChanged.connect(lambda: auto_preview(P, W, Conv))
    W.intExt.toggled.connect(lambda: auto_preview(P, W, Conv, 'intext'))
    W.centLeft.toggled.connect(lambda: auto_preview(P, W, Conv, 'center'))
    W.preview.pressed.connect(lambda: preview(P, W, Conv))
    W.add.pressed.connect(lambda: Conv.conv_add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda: Conv.conv_undo_shape(P, W))
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'wEntry', 'hEntry',
               'aEntry', 'r1Entry', 'r2Entry', 'r3Entry', 'r4Entry', ]
    for entry in entries:
        W[entry].textChanged.connect(lambda: entry_changed(P, W, Conv, W.sender()))
        W[entry].returnPressed.connect(lambda: preview(P, W, Conv))
    W.r1Button.pressed.connect(lambda: rad_button_pressed(P, W, Conv, W.sender(), '1'))
    W.r2Button.pressed.connect(lambda: rad_button_pressed(P, W, Conv, W.sender(), '2'))
    W.r3Button.pressed.connect(lambda: rad_button_pressed(P, W, Conv, W.sender(), '3'))
    W.r4Button.pressed.connect(lambda: rad_button_pressed(P, W, Conv, W.sender(), '4'))
    # add to layout
    if P.landscape:
        W.entries.addWidget(W.ctLabel, 0, 0)
        W.entries.addWidget(W.intExt, 0, 1)
        W.entries.addWidget(W.spLabel, 1, 0)
        W.entries.addWidget(W.centLeft, 1, 1)
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
        W.entries.addWidget(W.r1Button, 9, 0)
        W.entries.addWidget(W.r1Entry, 9, 1)
        W.entries.addWidget(W.r2Button, 9, 2)
        W.entries.addWidget(W.r2Entry, 9, 3)
        W.entries.addWidget(W.r3Button, 10, 0)
        W.entries.addWidget(W.r3Entry, 10, 1)
        W.entries.addWidget(W.r4Button, 10, 2)
        W.entries.addWidget(W.r4Entry, 10, 3)
        W.s11 = QLabel('')
        W.s11.setFixedHeight(24)
        W.entries.addWidget(W.s11, 11, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0, 2, 7, 3)
    else:
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.ctLabel, 1, 0)
        W.entries.addWidget(W.intExt, 1, 1)
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
        W.entries.addWidget(W.wLabel, 5, 0)
        W.entries.addWidget(W.wEntry, 5, 1)
        W.entries.addWidget(W.hLabel, 5, 2)
        W.entries.addWidget(W.hEntry, 5, 3)
        W.entries.addWidget(W.aLabel, 6, 0)
        W.entries.addWidget(W.aEntry, 6, 1)
        W.entries.addWidget(W.r1Button, 7, 0)
        W.entries.addWidget(W.r1Entry, 7, 1)
        W.entries.addWidget(W.r2Button, 7, 2)
        W.entries.addWidget(W.r2Entry, 7, 3)
        W.entries.addWidget(W.r3Button, 8, 0)
        W.entries.addWidget(W.r3Entry, 8, 1)
        W.entries.addWidget(W.r4Button, 8, 2)
        W.entries.addWidget(W.r4Entry, 8, 3)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0, 5, 7, 3)
    W.wEntry.setFocus()
    P.convSettingsChanged = False
