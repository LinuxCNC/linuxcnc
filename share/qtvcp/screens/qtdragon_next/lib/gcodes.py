#!/usr/bin/env python3
# Copyright (c) 2023 Jim Sloot (persei802@gmail.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import os
from qtvcp.lib import mdi_text as mdiText
from PyQt5 import QtWidgets, uic

HERE = os.path.dirname(os.path.abspath(__file__))

class GCodes(QtWidgets.QWidget):
    def __init__(self, widgets=None):
        super(GCodes, self).__init__()
        # Load the widgets UI file:
        self.filename = os.path.join(HERE, 'gcodes.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            print(f"Error: {e}")

    def setup_list(self):
        self.gcode_list.currentRowChanged.connect(self.list_row_changed)
        titles = mdiText.gcode_titles()
        for key in sorted(titles.keys()):
            self.gcode_list.addItem(key + ' ' + titles[key])

    def list_row_changed(self, row):
        line = self.gcode_list.currentItem().text()
        text = line.split(' ')[0]
        desc = mdiText.gcode_descriptions(text) or 'No Match'
        self.gcode_description.clear()
        self.gcode_description.insertPlainText(desc)

        if text != 'null':
            words = mdiText.gcode_words()
            if text in words:
                parm = text + ' '
                for index, value in enumerate(words[text], start=0):
                    parm += value
                self.gcode_parameters.setText(parm)
            else:
                self.gcode_parameters.setText('')
