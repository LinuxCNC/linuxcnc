'''
conv_circle.py

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
from plasmac import circle as CIRCLE

_translate = QCoreApplication.translate


def preview(P, W, Conv):
    if P.dialogError:
        return
    if not W.xsEntry.text():
        W.xsEntry.setText(f'{P.xOrigin:0.3f}')
    if not W.ysEntry.text():
        W.ysEntry.setText(f'{P.yOrigin:0.3f}')
    origin = W.centLeft.text() == 'CENTER'
    error = CIRCLE.preview(Conv, P.fTmp, P.fNgc, P.fNgcBkp,
                           int(W.conv_material.currentText().split(':')[0]),
                           W.conv_material.currentText().split(':')[1].strip(),
                           P.preAmble, P.postAmble,
                           W.liEntry.text(), W.loEntry.text(),
                           origin, W.xsEntry.text(), W.ysEntry.text(),
                           W.kerf_width.value(), P.intExt,
                           W.overcut.isChecked(), W.ocEntry.text(),
                           P.holeDiameter, P.holeSpeed,
                           W.dEntry.text(), P.invalidLeads)
    if error:
        P.dialogError = True
        P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Circle Error'), error)
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
        Conv.conv_entry_changed(P, W, None, 'circle')
    elif button == 'center':
        if not W.centLeft.isChecked():
            return
        Conv.conv_auto_preview_button(P, W, button)
    if W.main_tab_widget.currentIndex() == 1 and W.dEntry.text():
        preview(P, W, Conv)


def entry_changed(P, W, Conv, widget):
    Conv.conv_entry_changed(P, W, widget, 'circle')


def overcut_toggled(P, W, Conv):
    if W.overcut.isChecked():
        try:
            lolen = float(W.loEntry.text())
        except:
            lolen = 0
        try:
            dia = float(W.dEntry.text())
        except:
            dia = 0
        if (P.intExt and lolen) or not dia or dia > P.holeDiameter:
            W.overcut.setChecked(False)
    auto_preview(P, W, Conv)


def cut_type_toggled(P, W):
    try:
        dia = float(W.dEntry.text())
    except:
        dia = 0
    if dia >= P.holeDiameter or dia == 0 or P.intExt:
        W.overcut.setChecked(False)
        W.overcut.setEnabled(False)
        W.ocEntry.setEnabled(False)
    else:
        W.overcut.setEnabled(True)
        W.ocEntry.setEnabled(True)


def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(CIRCLE)
    W.dLabel.setText(_translate('Conversational', 'DIAMETER'))
    W.lDesc.setText(_translate('Conversational', 'CREATING CIRCLE'))
    W.iLabel.setPixmap(P.conv_circle_l)
    # alignment and size
    rightAlign = ['ctLabel', 'spLabel', 'xsLabel', 'xsEntry', 'ysLabel',
                  'ysEntry', 'liLabel', 'liEntry', 'loLabel', 'loEntry',
                  'dLabel', 'dEntry', 'ocLabel', 'ocEntry']
    centerAlign = ['lDesc']
    rButton = ['intExt', 'centLeft']
    pButton = ['preview', 'add', 'undo', 'overcut']
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
    W.overcut.toggled.connect(lambda: overcut_toggled(P, W, Conv))
    W.preview.pressed.connect(lambda: preview(P, W, Conv))
    W.add.pressed.connect(lambda: Conv.conv_add_shape_to_file(P, W))
    W.undo.pressed.connect(lambda: Conv.conv_undo_shape(P, W))
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'dEntry', 'ocEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda: entry_changed(P, W, Conv, W.sender()))
        W[entry].returnPressed.connect(lambda: preview(P, W, Conv))
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
        W.entries.addWidget(W.dLabel, 6, 0)
        W.entries.addWidget(W.dEntry, 6, 1)
        W.entries.addWidget(W.overcut, 7, 1)
        W.entries.addWidget(W.ocLabel, 8, 0)
        W.entries.addWidget(W.ocEntry, 8, 1)
        for r in [9, 10, 11]:
            W[f's{r}'] = QLabel('')
            W[f's{r}'].setFixedHeight(24)
            W.entries.addWidget(W[f's{r}'], r, 0)
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
        W.entries.addWidget(W.dLabel, 5, 0)
        W.entries.addWidget(W.dEntry, 5, 1)
        W.entries.addWidget(W.overcut, 6, 1)
        W.entries.addWidget(W.ocLabel, 6, 2)
        W.entries.addWidget(W.ocEntry, 6, 3)
        for r in [7, 8]:
            W[f's{r}'] = QLabel('')
            W[f's{r}'].setFixedHeight(24)
            W.entries.addWidget(W[f's{r}'], r, 0)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10, 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0, 5, 7, 3)
    W.dEntry.setFocus()
    P.convSettingsChanged = False
