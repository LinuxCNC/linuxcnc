#!/usr/bin/env python3
# qtVcp tab widgets with adjustable tab height
#
# Copyright (c) 2019  Chris Morley <chrisinnanaimo@hotmail.com>
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

from PyQt5.QtWidgets import (QTabWidget, QTabBar, QPushButton)
from PyQt5.QtCore import pyqtProperty, QSize

class TabWidget(QTabWidget):
    def __init__(self, parent=None):
        super(TabWidget, self).__init__(parent)
        self._tabSize = 1.5
        self.setTabBar(TabBar(size=self._tabSize))
        #self.setCornerWidget(QPushButton("hi"))

    def set_tabSize(self, data):
        self._tabSize = data
    def get_tabSize(self):
        return self._tabSize
    def reset_tabSize(self):
        self._tabSize = 1.5

    tabSize = pyqtProperty(float, get_tabSize, set_tabSize, reset_tabSize)

class TabBar(QTabBar):
    def __init__(self, parent=None, size=1.5):
        super(TabBar, self).__init__(parent)
        self._size=size

    def tabSizeHint(self, index):
        size = QTabBar.tabSizeHint(self, index)
        w = size.height()*self._size
        return QSize(size.width(), w)

