#!/usr/bin/env python

'''
conv_slot.py

Copyright (C) 2020  Phillip A Carter

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

from PyQt5.QtCore import Qt 
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup 
from PyQt5.QtGui import QPixmap 

def widgets(P, W):
    W.ctLabel = QLabel('Slot is not implemented yet')
    W.ctLabel.setAlignment(Qt.AlignCenter | Qt.AlignBottom)
    W.ctLabel.setFixedWidth(424)
    W.entries.addWidget(W.ctLabel, 0, 0, 1, 5)

    W.ctLabel = QLabel('You could do some circle work :-)')
    W.ctLabel.setAlignment(Qt.AlignCenter | Qt.AlignBottom)
    W.entries.addWidget(W.ctLabel, 1, 0, 1, 5)

    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_slot_l.png'.format(P.IMAGES)).scaledToWidth(240)
    W.iLabel.setPixmap(pixmap)
    W.entries.addWidget(W.iLabel, 2 , 2, 7, 3)
