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
    P.origin = W.center.isChecked()
    try:
        P.leadIn = float(W.liEntry.text())
    except:
        msg.append(_translate('Conversational', 'LEAD IN'))
    try:
        P.leadOut = float(W.loEntry.text())
    except:
        msg.append(_translate('Conversational', 'LEAD OUT'))
    try:
        P.holeDiameter = float(W.shEntry.text())
    except:
        msg.append(_translate('Conversational', 'DIAMETER'))
    try:
        P.holeSpeed = int(W.hsEntry.text())
    except:
        msg.append(_translate('Conversational', 'SPEED %'))
    try:
        P.gridSize = float(W.gsEntry.text())
    except:
        msg.append(_translate('Conversational', 'GRID SIZE'))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, '{}:\n\n{}'.format(msg0, msg1))
        return

    P.PREFS.putpref('Preamble', P.preAmble, str, 'CONVERSATIONAL')
    P.PREFS.putpref('Postamble', P.postAmble, str, 'CONVERSATIONAL')
    P.PREFS.putpref('Origin', P.origin, int, 'CONVERSATIONAL')
    P.PREFS.putpref('Leadin', P.leadIn, float, 'CONVERSATIONAL')
    P.PREFS.putpref('Leadout', P.leadOut, float, 'CONVERSATIONAL')
    P.PREFS.putpref('Hole diameter', P.holeDiameter, float, 'CONVERSATIONAL')
    P.PREFS.putpref('Hole speed', P.holeSpeed, int, 'CONVERSATIONAL')
    P.PREFS.putpref('Grid Size', P.gridSize, float, 'CONVERSATIONAL')
    show(P, W)
    P.convSettingsChanged = True
#    W[P.oldConvButton].click()

def error_set(P, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Scaling Error'), msg)

#def reload(parent, ambles, unitCode):
def reload(P, W, Conv):
    load(P, W)
    show(P, W)
    P.convSettingsChanged = True
#    W[P.oldConvButton].click()

def exit(P, W, Conv):
    Conv.conv_restore_buttons(P, W)
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
        W.center.setChecked(True)
    else:
        W.bLeft.setChecked(True)
    P.oSaved = P.origin
    # grid size is in inches
    W.conv_preview.grid_size = P.gridSize / P.unitsPerMm / 25.4
    W.conv_preview.set_current_view()

def widgets(P, W, Conv):
    W.shLabel.setText(_translate('Conversational', 'SMALL HOLES'))
    #alignment and size
    ra = ['preLabel', 'pstLabel', 'liLabel', 'liEntry', 'loEntry', \
          'hLabel', 'shEntry', 'hsEntry', 'gsLabel', 'gsEntry']
    la = ['loLabel', 'hsLabel']
    ca = ['oLabel', 'llLabel', 'shLabel', 'pvLabel']
    rb = ['center', 'bLeft']
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
    W.entries.addWidget(W.center, 3, 1)
    W.entries.addWidget(W.bLeft, 3, 3)
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
    W.entries.addWidget(W.save, 12, 0)
    W.entries.addWidget(W.reload, 12, 2)
    W.entries.addWidget(W.cExit, 12, 4)
    for blank in range(2):
        W['{}'.format(blank)] = QLabel('')
        W.entries.addWidget(W['{}'.format(blank)], 10 + blank, 0)
    W.preEntry.setFocus()
