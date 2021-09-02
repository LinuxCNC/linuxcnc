#!/usr/bin/env python3

from . import mdi_text as mdiText
from qtvcp.qt_makegui import VCPWindow

WIDGETS = VCPWindow()


class GCodes:
    def __init__(self, widgets=None):
        self.w = WIDGETS

    def setup_list(self):
        self.w.gcode_list.currentRowChanged.connect(self.list_row_changed)
        titles = mdiText.gcode_titles()
        for key in sorted(titles.keys()):
            self.w.gcode_list.addItem(key + ' ' + titles[key])
        self.w.gcode_description.setReadOnly(True)

    def list_row_changed(self, row):
        line = self.w.gcode_list.currentItem().text()
        print(line)
        text = line.split(' ')[0]
        desc = mdiText.gcode_descriptions(text) or 'No Match'
        self.w.gcode_description.clear()
        self.w.gcode_description.insertPlainText(desc)

        if text != 'null':
            words = mdiText.gcode_words()
            if text in words:
                parm = text + ' '
                for index, value in enumerate(words[text], start=0):
                    parm += value
                self.w.gcode_parameters.setText(parm)
            else:
                self.w.gcode_parameters.setText('')

    def mdiClear(self):
        for index in range(1, 8):
            getattr(parent, 'gcodeParameter_' + str(index)).setText('')
        self.w.gcode_description.setText('')
        self.w.gcodeHelpLabel.setText('')
