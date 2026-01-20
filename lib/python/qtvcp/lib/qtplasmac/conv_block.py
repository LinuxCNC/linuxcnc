'''
conv_block.py

Copyright (C) 2020 - 2025 Phillip A Carter
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
from qtvcp.core import Status
from importlib import reload
from plasmac import block as BLOCK

_translate = QCoreApplication.translate

STATUS = Status()


def preview(P, W, Conv, doMirror=False, doFlip=False):
    if P.dialogError or not W.preview.isEnabled():
        return
    error = BLOCK.preview(Conv, P.fNgc, P.isConvBlock, W.conv_preview, W.cnEntry.text(), W.rnEntry.text(),
                          W.csEntry.text(), W.rsEntry.text(), W.oxEntry.text(), W.oyEntry.text(),
                          W.aEntry.text(), W.scEntry.text(), W.rtEntry.text(), doMirror, doFlip)
    if error:
        P.dialogError = True
        P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Block Error'), error)
    else:
        W.conv_preview.load(P.fNgc)
        W.conv_preview.set_current_view()
        W.add.setEnabled(True)
        W.undo.setEnabled(True)
        Conv.conv_preview_button(P, W, True)
        W.add.setEnabled(True)
        if not P.isConvBlock:
            P.isConvBlock = 'created'


def get_parameters(P, W):
    if P.isConvBlock:
        with open(P.fNgc, 'r') as inCode:
            line = inCode.readline()
            inputs = None
            for line in inCode:
                if ';CONV_BLOCK INPUTS' in line:
                    inputs = [line]
                elif inputs:
                    inputs = line.strip().strip(';').split(',')
                    W.csEntry.setText(inputs[0])
                    W.rsEntry.setText(inputs[1])
                    W.cnEntry.setText(inputs[2])
                    W.rnEntry.setText(inputs[3])
                    W.oxEntry.setText(inputs[4])
                    W.oyEntry.setText(inputs[5])
                    W.aEntry.setText(inputs[6])
                    W.scEntry.setText(inputs[7])
                    W.rtEntry.setText(inputs[8])
                    P.convMirror = int(inputs[9])
                    P.convFlip = int(inputs[10])
                    break


def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(BLOCK)
    W.shLabel.setText(_translate('Conversational', 'SHAPE'))
    W.lDesc.setText(_translate('Conversational', 'CREATE ARRAY OF SHAPES'))
    # alignment and size
    rightAlign = ['cnLabel', 'cnEntry', 'csEntry',
                  'rnLabel', 'rnEntry', 'rsEntry',
                  'oxLabel', 'oxEntry', 'oyEntry',
                  'scLabel', 'aEntry', 'scEntry',
                  'rtEntry']
    leftAlign = ['csLabel', 'rsLabel', 'oyLabel', 'aLabel', 'rtLabel']
    centerAlign = ['lDesc', 'ccLabel', 'rrLabel', 'oLabel', 'shLabel', 'ptLabel']
    pButton = ['preview', 'add', 'undo', 'mirror', 'flip']
    for widget in rightAlign:
        W[widget].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in leftAlign:
        W[widget].setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in centerAlign:
        W[widget].setAlignment(Qt.AlignCenter | Qt.AlignVCenter)
        if widget == 'lDesc':
            W[widget].setFixedWidth(240)
        else:
            W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in pButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    # starting parameters
    W.add.setEnabled(False)
    # connections
    W.preview.pressed.connect(lambda: preview(P, W, Conv))
    W.add.pressed.connect(lambda: Conv.conv_accept(P, W))
    W.undo.pressed.connect(lambda: Conv.conv_undo_shape(P, W))
    W.mirror.clicked.connect(lambda: preview(P, W, Conv, doMirror=True))
    W.flip.clicked.connect(lambda: preview(P, W, Conv, doFlip=True))
    entries = ['cnEntry', 'csEntry', 'rnEntry', 'rsEntry', 'oxEntry',
               'oyEntry', 'scEntry', 'aEntry', 'rtEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda: Conv.conv_entry_changed(P, W, W.sender()))
        W[entry].returnPressed.connect(lambda: preview(P, W, Conv))
    # add to layout
    if P.landscape:
        W.s0 = QLabel('')
        W.s0.setFixedHeight(51)
        W.entries.addWidget(W.s0, 0, 0)
        W.entries.addWidget(W.ccLabel, 1, 2)
        W.entries.addWidget(W.cnLabel, 1, 0)
        W.entries.addWidget(W.cnEntry, 1, 1)
        W.entries.addWidget(W.csEntry, 1, 3)
        W.entries.addWidget(W.csLabel, 1, 4)
        W.entries.addWidget(W.rrLabel, 3, 2)
        W.entries.addWidget(W.rnLabel, 3, 0)
        W.entries.addWidget(W.rnEntry, 3, 1)
        W.entries.addWidget(W.rsEntry, 3, 3)
        W.entries.addWidget(W.rsLabel, 3, 4)
        W.entries.addWidget(W.oLabel, 5, 2)
        W.entries.addWidget(W.oxLabel, 5, 0)
        W.entries.addWidget(W.oxEntry, 5, 1)
        W.entries.addWidget(W.oyEntry, 5, 3)
        W.entries.addWidget(W.oyLabel, 5, 4)
        W.entries.addWidget(W.ptLabel, 7, 2)
        W.entries.addWidget(W.aEntry, 7, 3)
        W.entries.addWidget(W.aLabel, 7, 4)
        W.entries.addWidget(W.scLabel, 9, 0)
        W.entries.addWidget(W.scEntry, 9, 1)
        W.entries.addWidget(W.shLabel, 9, 2)
        W.entries.addWidget(W.rtEntry, 9, 3)
        W.entries.addWidget(W.rtLabel, 9, 4)
        W.entries.addWidget(W.mirror, 11, 1)
        W.entries.addWidget(W.flip, 11, 3)
        for r in [2, 4, 6, 8, 10]:
            W[f'{r}'] = QLabel('')
            W[f'{r}'].setFixedHeight(24)
            W.entries.addWidget(W[f'{r}'], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13, 1, 1, 3)
    else:
        W.entries.addWidget(W.ccLabel, 0, 2)
        W.entries.addWidget(W.cnLabel, 0, 0)
        W.entries.addWidget(W.cnEntry, 0, 1)
        W.entries.addWidget(W.csEntry, 0, 3)
        W.entries.addWidget(W.csLabel, 0, 4)
        W.entries.addWidget(W.rrLabel, 2, 2)
        W.entries.addWidget(W.rnLabel, 2, 0)
        W.entries.addWidget(W.rnEntry, 2, 1)
        W.entries.addWidget(W.rsEntry, 2, 3)
        W.entries.addWidget(W.rsLabel, 2, 4)
        W.entries.addWidget(W.oLabel, 4, 2)
        W.entries.addWidget(W.oxLabel, 4, 0)
        W.entries.addWidget(W.oxEntry, 4, 1)
        W.entries.addWidget(W.oyEntry, 4, 3)
        W.entries.addWidget(W.oyLabel, 4, 4)
        W.entries.addWidget(W.ptLabel, 6, 2)
        W.entries.addWidget(W.aEntry, 6, 3)
        W.entries.addWidget(W.aLabel, 6, 4)
        W.entries.addWidget(W.scLabel, 8, 0)
        W.entries.addWidget(W.scEntry, 8, 1)
        W.entries.addWidget(W.shLabel, 8, 2)
        W.entries.addWidget(W.rtEntry, 8, 3)
        W.entries.addWidget(W.rtLabel, 8, 4)
        W.entries.addWidget(W.mirror, 9, 1)
        W.entries.addWidget(W.flip, 9, 3)
        for r in [1, 3, 5, 7]:
            W[f'{r}'] = QLabel('')
            W[f'{r}'].setFixedHeight(24)
            W.entries.addWidget(W[f'{r}'], r, 0)
        W.entries.addWidget(W.preview, 10, 0)
        W.entries.addWidget(W.add, 10, 2)
        W.entries.addWidget(W.undo, 10, 4)
    W.cnEntry.setFocus()
    P.convSettingsChanged = False
    get_parameters(P, W)
    if not float(W.csEntry.text()):
        W.csEntry.setText(f"{round(float(W.conv_preview.gcode_properties['x'].split()[4]) + (float(W.liEntry.text()) * 2), 2)}")
    if not float(W.rsEntry.text()):
        W.rsEntry.setText(f"{round(float(W.conv_preview.gcode_properties['y'].split()[4]) + (float(W.liEntry.text()) * 2), 2)}")
