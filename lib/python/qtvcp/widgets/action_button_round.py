#!/usr/bin/env python
# Qtvcp widget
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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

# This is an extention of action buttons to show a round button image.
# Action buttons are used to control linuxcnc behaivor by pressing.
# By making the button 'checkable' in the designer editor,
# the buton will toggle.
# In the designer editor, it is possible to select what the button will do.
###############################################################################

from PyQt5 import QtGui, QtCore

from qtvcp.widgets.action_button import ActionButton

class RoundButton(ActionButton):
    def __init__(self, parent=None):
        super(RoundButton, self).__init__(parent)
        self.setCheckable(True)
        self.setWindowFlags(QtCore.Qt.FramelessWindowHint)
        self.pixmap = QtGui.QPixmap()
        self.false_pixmap = QtGui.QPixmap()
        self.clip_region = QtGui.QRegion(QtCore.QRect(5, 5, self.width()-10, self.height()-10), QtGui.QRegion.Ellipse)

    def action(self, state):
        super(RoundButton, self).action(state)
        if state:
            self.clearMask()
        else:
            self.setMask(self.clip_region)

    def resizeEvent(self, event):
        self.clip_region = QtGui.QRegion(QtCore.QRect(5, 5, self.width()-10, self.height()-10), QtGui.QRegion.Ellipse)
        if not self.isChecked():
            self.setMask(self.clip_region)

    def paintEvent(self, event):
        p = QtGui.QPainter(self)
        if self.isChecked():
            if not self.pixmap.isNull():
                p.drawPixmap(event.rect(), self.pixmap)
                return
        else:
            if not self.false_pixmap.isNull():
                p.drawPixmap(event.rect(), self.false_pixmap)
                return
        super(RoundButton, self).paintEvent(event)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #########################################################################

    def setImagePath(self, data):
        self.pixmap = data
    def getImagePath(self):
        return self.pixmap
    def resetImagePath(self):
        self.pixmap

    def setFalseImagePath(self, data):
        self.false_pixmap = data
    def getFalseImagePath(self):
        return self.false_pixmap
    def resetFalseImagePath(self):
        self.false_pixmap

    image_path = QtCore.pyqtProperty(QtGui.QPixmap, getImagePath, setImagePath, resetImagePath)
    false_image_path = QtCore.pyqtProperty(QtGui.QPixmap, getFalseImagePath, setFalseImagePath, resetFalseImagePath)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = RoundButton()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()

