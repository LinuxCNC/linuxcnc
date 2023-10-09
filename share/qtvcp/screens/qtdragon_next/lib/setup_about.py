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
import sys
import os

from PyQt5 import QtCore, QtWidgets
from PyQt5.QtWidgets import QPushButton, QSizePolicy
from PyQt5.QtWebEngineWidgets import QWebEngineView
from PyQt5.QtWebEngineWidgets import QWebEnginePage

from qtvcp.core import Info, Path
from qtvcp import logger

LOG = logger.getLogger(__name__)
LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL
INFO = Info()
PATH = Path()
HELP = os.path.join(PATH.SCREENDIR, PATH.BASEPATH, "help_files")

# status message alert levels
DEFAULT =  0
WARNING =  1
CRITICAL = 2

# this class provides an overloaded function to disable navigation links
class WebPage(QWebEnginePage):
    def acceptNavigationRequest(self, url, navtype, mainframe):
        if navtype == self.NavigationTypeLinkClicked: return False
        return super().acceptNavigationRequest(url, navtype, mainframe)


class Setup_About():
    def __init__(self, widgets, parent):
        self.w = widgets
        self.parent = parent
        self.btn_idx = 0
        self.about_btns = []
        self.num_about_btns = 0
        self.max_about_btns = 8
        self.scroll_window = {'left': 0, 'right': 0}
        self.sizePolicy = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)

    def init_about(self):
        self.make_button('intro', 'ABOUT')
        self.make_button('vfd', 'USING\nA VFD')
        self.make_button('spindle_pause', 'SPINDLE\nPAUSE')
        self.make_button('mpg', 'USING\n A MPG')
        self.make_button('runfromline', 'RUN FROM\nLINE')
        self.make_button('stylesheets', 'STYLESHEETS')
        if 'A' in INFO.AVAILABLE_AXES:
            self.make_button('rotary_axis', 'ROTARY\nAXIS')
        self.make_button('custom', 'CUSTOM\nPANELS')
        self.num_about_btns = len(self.about_btns)
        self['btn_' + self.about_btns[0]].setChecked(True)

        self.scroll_window['left'] = 0
        self.scroll_window['right'] = self.max_about_btns - 1

        self.web_view_about = QWebEngineView()
        self.web_page_about = WebPage()
        self.web_view_about.setPage(self.web_page_about)
        self.w.layout_about_pages.addWidget(self.web_view_about)

        if self.num_about_btns <= self.max_about_btns:
            self.w.btn_about_left.hide()
            self.w.btn_about_right.hide()
        else:
            self.slide_scroll_window()
        self.w.about_buttonGroup.buttonClicked.connect(self.about_tab_changed)
        self.show_defaults()

    def make_button(self, name, title):
        self['btn_' + name] = QPushButton(title)
        self['btn_' + name].setSizePolicy(self.sizePolicy)
        self['btn_' + name].setMinimumSize(QtCore.QSize(90, 0))
        self['btn_' + name].setCheckable(True)
        self['btn_' + name].setProperty('html', name)
        self.w.about_buttonGroup.addButton(self['btn_' + name])
        self.w.layout_about_btns.insertWidget(self.btn_idx + 1, self['btn_' + name])
        self.about_btns.append(name)
        self.btn_idx += 1


    def show_defaults(self):
        try:
            fname = os.path.join(HELP, 'about_intro.html')
            url = QtCore.QUrl("file:///" + fname)
            self.web_page_about.load(url)
        except Exception as e:
            self.parent.add_status(f"Could not find default ABOUT page - {e}", CRITICAL)

    def about_tab_changed(self, btn):
        if btn == self.w.btn_about_left:
            self.scroll_left()
        elif btn == self.w.btn_about_right:
            self.scroll_right()
        else:
            html = btn.property('html')
            fname = os.path.join(HELP, 'about_' + html + '.html')
            if os.path.dirname(fname):
                url = QtCore.QUrl("file:///" + fname)
                self.web_page_about.load(url)
            else:
                self.parent.add_status(f"About file {fname} not found")

    def scroll_left(self):
        if self.scroll_window['left'] > 0:
            self.scroll_window['left'] -= 1
            self.scroll_window['right'] -= 1
            self.slide_scroll_window()

    def scroll_right(self):
        if self.scroll_window['right'] < self.num_about_btns - 1:
            self.scroll_window['left'] += 1
            self.scroll_window['right'] += 1
            self.slide_scroll_window()

    def slide_scroll_window(self):
        for item in self.about_btns:
            index = self['btn_' + item].property('index')
            if index < self.scroll_window['left'] or index > self.scroll_window['right']:
                self['btn_' + item].hide()
            else:
                self['btn_' + item].show()

    # required code for subscriptable objects
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = Setup_About()
    w.show()
    sys.exit( app.exec_() )
