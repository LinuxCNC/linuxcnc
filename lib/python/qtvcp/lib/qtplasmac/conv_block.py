'''
conv_block.py

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
from qtvcp.core import Status
from importlib import reload
from plasmac import block as BLOCK

_translate = QCoreApplication.translate

STATUS = Status()

def preview(P, W, Conv):
    if P.dialogError or not W.preview.isEnabled():
        return
    error = BLOCK.preview(Conv, P.fNgc, P.fTmp, \
            W.cnEntry.text(), W.rnEntry.text(), W.coEntry.text(), W.roEntry.text(), \
            W.oxEntry.text(), W.oyEntry.text(), W.aEntry.text(), W.scEntry.text(), \
            W.rtEntry.text(), P.convBlock, P.convMirror, P.convFlip, \
            P.convMirrorToggle, P.convFlipToggle, STATUS.stat.g5x_index, P.convUnits)
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
        P.convBlock[0] = True
        P.convMirrorToggle = False
        P.convFlipToggle = False

def mirror_shape(P, W, Conv):
    if P.convMirror == 1:
        P.convMirror = -1
    else:
        P.convMirror = 1
    P.convMirrorToggle = True
    preview(P, W, Conv)

def flip_shape(P, W, Conv):
    if P.convFlip == 1:
        P.convFlip = -1
    else:
        P.convFlip = 1
    P.convFlipToggle = True
    preview(P, W, Conv)

def undo_shape(P, W, Conv):
    P.convMirror = 1
    P.convMirrorToggle = False
    P.convFlip = 1
    P.convFlipToggle = False
    Conv.conv_undo_shape(P, W)

def get_parameters(P, W):
    P.wcs_rotation('get')
    inCode = open(P.fNgc, 'r')
    P.convBlock = [False, False]
    P.convUnits = [1, None]
    P.convMirror = 1
    P.convMirrorToggle = False
    P.convFlip = 1
    P.convFlipToggle = False
    for line in inCode:
        line = line.strip().lower()
        # maybe check here for old style rotate, scale, and array
        if line.startswith(';conversational block'):
            P.convBlock = [True, True]
        elif 'G21' in line.upper().replace(' ', '') and P.unitsPerMm != 1:
            P.convUnits = [25.4, 'G21']
        elif 'G20' in line.upper().replace(' ', '') and P.unitsPerMm == 1:
            P.convUnits = [0.03937, 'G20']
        elif 'm3' in line:
            break
    inCode.seek(0, 0)
    if P.convBlock[0]:
        for line in inCode:
            line = line.strip().lower()
            if line.startswith('#<array_x_offset>'):
                W.coEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<array_y_offset>'):
                W.roEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<array_columns>'):
                W.cnEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<array_rows>'):
                W.rnEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<origin_x_offset>'):
                W.oxEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<origin_y_offset>'):
                W.oyEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<array_angle>'):
                W.aEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<blk_scale>'):
                W.scEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<shape_angle>'):
                W.rtEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<shape_mirror>'):
                P.convMirror = int(line.split('=')[1].strip())
            elif line.startswith('#<shape_flip>'):
                P.convFlip = int(line.split('=')[1].strip())
            elif 'm3' in line:
                break
    inCode.seek(0, 0)

def widgets(P, W, Conv):
    if P.developmentPin.get():
        reload(BLOCK)
    W.shLabel.setText(_translate('Conversational', 'SHAPE'))
    W.lDesc.setText(_translate('Conversational', 'CREATE ARRAY OF SHAPES'))
    #alignment and size
    rightAlign = ['cnLabel', 'cnEntry', 'coEntry', \
                  'rnLabel', 'rnEntry', 'roEntry', \
                  'oxLabel', 'oxEntry', 'oyEntry', \
                  'scLabel', 'aEntry', 'scEntry', \
                  'rtEntry']
    leftAlign = ['coLabel', 'roLabel', 'oyLabel', 'aLabel', 'rtLabel']
    centerAlign = ['lDesc', 'ccLabel', 'rrLabel', 'oLabel','shLabel','ptLabel']
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
    #starting parameters
    W.add.setEnabled(False)
    #connections
    W.preview.pressed.connect(lambda:preview(P, W, Conv))
    W.add.pressed.connect(lambda:Conv.conv_accept(P, W))
    W.undo.pressed.connect(lambda:undo_shape(P, W, Conv))
    W.mirror.clicked.connect(lambda:mirror_shape(P, W, Conv))
    W.flip.clicked.connect(lambda:flip_shape(P, W, Conv))
    entries = ['cnEntry', 'coEntry', 'rnEntry', 'roEntry', 'oxEntry',
               'oyEntry', 'scEntry', 'aEntry', 'rtEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:Conv.conv_entry_changed(P, W, W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W, Conv))
    #add to layout
    if P.landscape:
        W.s0 = QLabel('')
        W.s0.setFixedHeight(51)
        W.entries.addWidget(W.s0, 0, 0)
        W.entries.addWidget(W.ccLabel, 1, 2)
        W.entries.addWidget(W.cnLabel, 1, 0)
        W.entries.addWidget(W.cnEntry, 1, 1)
        W.entries.addWidget(W.coEntry, 1, 3)
        W.entries.addWidget(W.coLabel, 1, 4)
        W.entries.addWidget(W.rrLabel, 3, 2)
        W.entries.addWidget(W.rnLabel, 3, 0)
        W.entries.addWidget(W.rnEntry, 3, 1)
        W.entries.addWidget(W.roEntry, 3, 3)
        W.entries.addWidget(W.roLabel, 3, 4)
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
        for r in [2,4,6,8,10]:
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
    else:
        W.entries.addWidget(W.ccLabel, 0, 2)
        W.entries.addWidget(W.cnLabel, 0, 0)
        W.entries.addWidget(W.cnEntry, 0, 1)
        W.entries.addWidget(W.coEntry, 0, 3)
        W.entries.addWidget(W.coLabel, 0, 4)
        W.entries.addWidget(W.rrLabel, 2, 2)
        W.entries.addWidget(W.rnLabel, 2, 0)
        W.entries.addWidget(W.rnEntry, 2, 1)
        W.entries.addWidget(W.roEntry, 2, 3)
        W.entries.addWidget(W.roLabel, 2, 4)
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
        for r in [1,3,5,7]:
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 10, 0)
        W.entries.addWidget(W.add, 10, 2)
        W.entries.addWidget(W.undo, 10, 4)
    W.cnEntry.setFocus()
    P.convSettingsChanged = False
    get_parameters(P, W)
