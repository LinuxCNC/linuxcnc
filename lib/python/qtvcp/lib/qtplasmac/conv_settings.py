'''
conv_settings.py

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

_translate = QCoreApplication.translate

def save(P, W, Conv):
    if P.dialogError: return
    msg = []
    P.preAmble = W.preEntry.text()
    P.postAmble = W.pstEntry.text()
    P.origin = W.centLeft.text() == 'CENTER'
    error = ''
    valid, P.leadIn = Conv.conv_is_float(W.liEntry.text())
    if not valid:
        msg = _translate('Conversational', 'Invalid LEAD IN entry detected')
        error += '{}\n\n'.format(msg)
    valid, P.leadOut = Conv.conv_is_float(W.loEntry.text())
    if not valid:
        msg = _translate('Conversational', 'Invalid LEAD OUT entry detected')
        error += '{}\n\n'.format(msg)
    valid, P.holeDiameter = Conv.conv_is_float(W.shEntry.text())
    if not valid:
        msg = _translate('Conversational', 'Invalid DIAMETER entry detected')
        error += '{}\n\n'.format(msg)
    valid, P.holeSpeed = Conv.conv_is_float(W.hsEntry.text())
    if not valid:
        msg = _translate('Conversational', 'Invalid SPEED % entry detected')
        error += '{}\n\n'.format(msg)
    valid, P.gridSize = Conv.conv_is_float(W.gsEntry.text())
    if not valid:
        msg = _translate('Conversational', 'Invalid GRID SIZE entry detected')
        error += '{}\n\n'.format(msg)
    if error:
        P.dialogError = True
        P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Settings Error'), error)
        return
    P.PREFS.putpref('Preamble', P.preAmble, str, 'CONVERSATIONAL')
    P.PREFS.putpref('Postamble', P.postAmble, str, 'CONVERSATIONAL')
    P.PREFS.putpref('Origin', P.origin, int, 'CONVERSATIONAL')
    P.PREFS.putpref('Leadin', P.leadIn, float, 'CONVERSATIONAL')
    P.PREFS.putpref('Leadout', P.leadOut, float, 'CONVERSATIONAL')
    P.PREFS.putpref('Hole diameter', P.holeDiameter, float, 'CONVERSATIONAL')
    P.PREFS.putpref('Hole speed', P.holeSpeed, int, 'CONVERSATIONAL')
    P.PREFS.putpref('Grid Size', P.gridSize, float, 'CONVERSATIONAL')
    # grid size is in inches
    W.conv_preview.grid_size = P.gridSize / P.unitsPerMm / 25.4
    W.conv_preview.set_current_view()
    P.convSettingsChanged = 1

def reload(P, W, Conv):
    load(P, W)
    show(P, W)
    if not P.convSettingsChanged:
        P.convSettingsChanged = 2

def exit(P, W, Conv):
    if P.convSettingsChanged != 1:
        W.centLeft.setText(P.savedSettings['origin'])
        W.intExt.setText(P.savedSettings['intext'])
        W.liEntry.setText(P.savedSettings['in'])
        W.loEntry.setText(P.savedSettings['out'])
    Conv.conv_restore_buttons(P, W)
    if not P.convSettingsChanged:
        P.convSettingsChanged = 3
    W[P.oldConvButton].click()

def load(P, W):
    P.preAmble = P.PREFS.getpref('Preamble', P.ambles, str, 'CONVERSATIONAL')
    P.postAmble = P.PREFS.getpref('Postamble', P.ambles, str, 'CONVERSATIONAL')
    P.origin = P.PREFS.getpref('Origin', 0, int, 'CONVERSATIONAL')
    P.leadIn = P.PREFS.getpref('Leadin', 0, float, 'CONVERSATIONAL')
    P.leadOut = P.PREFS.getpref('Leadout', 0, float, 'CONVERSATIONAL')
    P.holeDiameter = P.PREFS.getpref('Hole diameter', P.unitCode[2], float, 'CONVERSATIONAL')
    P.holeSpeed = P.PREFS.getpref('Hole speed', 60, int, 'CONVERSATIONAL')
    P.gridSize = P.PREFS.getpref('Grid Size', 0, float, 'CONVERSATIONAL')

def show(P, W):
    W.preEntry.setText(P.preAmble)
    W.pstEntry.setText(P.postAmble)
    W.liEntry.setText('{}'.format(P.leadIn))
    W.loEntry.setText('{}'.format(P.leadOut))
    W.shEntry.setText('{}'.format(P.holeDiameter))
    W.hsEntry.setText('{}'.format(P.holeSpeed))
    W.gsEntry.setText('{}'.format(P.gridSize))
    if P.origin:
        W.centLeft.setText('CENTER')
    else:
        W.centLeft.setText('BTM LEFT')
    # grid size is in inches
    W.conv_preview.grid_size = P.gridSize / P.unitsPerMm / 25.4
    W.conv_preview.set_current_view()

def centLeft_toggled(P, W, Conv):
    if not W.centLeft.isChecked():
        return
    text = 'CENTER' if W.centLeft.text() == 'BTM LEFT' else 'BTM LEFT'
    W.centLeft.setText(text)
    W.centLeft.setChecked(False)

def widgets(P, W, Conv):
    W.shLabel.setText(_translate('Conversational', 'SMALL HOLES'))
    W.hLabel.setText(_translate('Conversational', 'DIAMETER'))
    #alignment and size
    ra = ['preLabel', 'pstLabel', 'liLabel', 'liEntry', 'loEntry', \
          'hLabel', 'shEntry', 'hsEntry', 'gsLabel', 'gsEntry']
    la = ['loLabel', 'hsLabel']
    ca = ['oLabel', 'llLabel', 'shLabel', 'pvLabel']
    rb = ['centLeft']
    bt = ['save', 'reload', 'cExit']
    for w in ra:
        W[w].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        W[w].setFixedWidth(80)
        W[w].setFixedHeight(24)
    for w in la:
        W[w].setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        W[w].setFixedWidth(80)
        W[w].setFixedHeight(24)
    for w in ca:
        W[w].setAlignment(Qt.AlignCenter | Qt.AlignBottom)
        W[w].setFixedWidth(240)
        W[w].setFixedHeight(24)
    for w in rb:
        W[w].setFixedWidth(80)
        W[w].setFixedHeight(24)
    for w in bt:
        W[w].setFixedWidth(80)
        W[w].setFixedHeight(24)
    # connections
    W.centLeft.toggled.connect(lambda:centLeft_toggled(P, W, Conv))
    W.liEntry.textChanged.connect(lambda:Conv.conv_entry_changed(P, W, W.sender()))
    W.loEntry.textChanged.connect(lambda:Conv.conv_entry_changed(P, W, W.sender()))
    W.shEntry.textChanged.connect(lambda:Conv.conv_entry_changed(P, W, W.sender()))
    W.hsEntry.textChanged.connect(lambda:Conv.conv_entry_changed(P, W, W.sender()))
    W.gsEntry.textChanged.connect(lambda:Conv.conv_entry_changed(P, W, W.sender()))
    W.save.pressed.connect(lambda:save(P, W, Conv))
    W.reload.pressed.connect(lambda:reload(P, W, Conv))
    W.cExit.pressed.connect(lambda:exit(P, W, Conv))
    #add to layout
    W.entries.addWidget(W.preLabel, 0, 0)
    W.entries.addWidget(W.preEntry, 0, 1, 1, 4)
    W.entries.addWidget(W.pstLabel, 1, 0)
    W.entries.addWidget(W.pstEntry, 1, 1, 1, 4)
    W.entries.addWidget(W.oLabel, 2, 1, 1, 3)
    W.entries.addWidget(W.centLeft, 3, 2)
    W.entries.addWidget(W.llLabel, 4, 1, 1, 3)
    W.entries.addWidget(W.liLabel, 5, 0)
    W.entries.addWidget(W.liEntry, 5, 1)
    W.entries.addWidget(W.loEntry, 5, 3)
    W.entries.addWidget(W.loLabel, 5, 4)
    W.entries.addWidget(W.shLabel, 6, 1, 1, 3)
    W.entries.addWidget(W.hLabel, 7, 0)
    W.entries.addWidget(W.shEntry, 7, 1)
    W.entries.addWidget(W.hsEntry, 7, 3)
    W.entries.addWidget(W.hsLabel, 7, 4)
    W.entries.addWidget(W.pvLabel, 8, 1, 1, 3)
    W.entries.addWidget(W.gsLabel, 9, 0)
    W.entries.addWidget(W.gsEntry, 9, 1)
    for r in (10,11):
        W['s{}'.format(r)] = QLabel('')
        W['s{}'.format(r)].setFixedHeight(24)
        W.entries.addWidget(W['s{}'.format(r)], r, 0)
    W.entries.addWidget(W.save, 12, 0)
    W.entries.addWidget(W.reload, 12, 2)
    W.entries.addWidget(W.cExit, 12, 4)
    W.preEntry.setFocus()
