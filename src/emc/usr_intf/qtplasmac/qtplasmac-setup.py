
'''
qtplasmac-setup.py

This file is used to switch a QtPlasmaC configuration
from RIP to Package or vice versa.

Copyright (C) 2020, 2021 Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

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

import os
import sys
import time
from shutil import copy as COPY
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *

class ConfSwitch(QMainWindow, object):

# INITIALISATION
    def __init__(self, parent=None):
        super(ConfSwitch, self).__init__(parent)
        self.appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        if 'usr/' in self.appPath:
            self.commonPath = '/usr/share/doc/linuxcnc/examples/sample-configs/by_machine/qtplasmac/qtplasmac/'
            confType = "package"
        else:
            self.commonPath = self.appPath.replace('bin', 'configs/by_machine/qtplasmac/qtplasmac/')
            confType = "run in place"
        self.setFixedWidth(500)
        wid = QWidget(self)
        qtRectangle = self.frameGeometry()
        centerPoint = QDesktopWidget().availableGeometry().center()
        qtRectangle.moveCenter(centerPoint)
        self.move(qtRectangle.topLeft())
        self.setCentralWidget(wid)
        layout = QHBoxLayout()
        wid.setLayout(layout)
        self.setWindowTitle('QtPlasmaC Switcher')
        vBox = QVBoxLayout()
        heading  = 'Switch QtPlasmaC installation type to:\n'
        headerLabel = QLabel(heading)
        headerLabel.setAlignment(Qt.AlignCenter)
        vBox.addWidget(headerLabel)
        info  = '{} Installation\n'.format(confType)
        infoLabel = QLabel(info)
        infoLabel.setAlignment(Qt.AlignCenter)
        vBox.addWidget(infoLabel)
        fromLabel = QLabel('INI file in existing QtPlasmaC config:')
        fromLabel.setAlignment(Qt.AlignBottom)
        vBox.addWidget(fromLabel)
        fromFileHBox = QHBoxLayout()
        fromFileButton = QPushButton('SELECT')
        self.fromFile = QLineEdit()
        self.fromFile.setEnabled(False)
        fromFileHBox.addWidget(fromFileButton)
        fromFileHBox.addWidget(self.fromFile)
        vBox.addLayout(fromFileHBox)
        vSpace2 = QSpacerItem(40, 40, QSizePolicy.Minimum, QSizePolicy.Minimum)
        vBox.addItem(vSpace2)
        buttonHBox = QHBoxLayout()
        switch = QPushButton('SWITCH')
        buttonHBox.addWidget(switch)
        hSpace1 = QSpacerItem(40, 40, QSizePolicy.Expanding, QSizePolicy.Minimum)
        buttonHBox.addItem(hSpace1)
        cancel = QPushButton('EXIT')
        buttonHBox.addWidget(cancel)
        vBox.addLayout(buttonHBox)
        layout.addLayout(vBox)
        self.setStyleSheet( \
            'QWidget {color: #ffee06; background: #16160e} \
             QLabel {height: 20} \
             QPushButton {border: 1 solid #ffee06; border-radius: 4; height: 40; width: 80} \
             QFileDialog QPushButton {border: 1 solid #ffee06; border-radius: 4; height: 30; margin: 6} \
             QPushButton:pressed {color: #16160e; background: #ffee06} \
             QLineEdit {border: 1 solid #ffee06; border-radius: 4; height: 40} \
             QFileDialog QLineEdit {border: 1 solid #ffee06; border-radius: 4; height: 30} \
             QTableView::item:selected:active {color: #16160e; background-color: #ffee06} \
             QTableView::item:selected:!active {color: #16160e; background-color: #ffee06} \
             QHeaderView::section {color: #ffee06; background-color: #36362e; border: 1 solid #ffee06; border-radius: 4; margin: 2} \
             QComboBox {color: #ffee06; background-color: #16160e; border: 1 solid #ffee06; border-radius: 4; height: 30} \
             QComboBox::drop-down {width: 0} \
             QComboBox QListView {border: 4p solid #ffee06; border-radius: 0} \
             QComboBox QAbstractItemView {border: 2px solid #ffee06; border-radius: 4} \
             QScrollBar:horizontal {background: #36362e; border: 0; border-radius: 4; margin: 0; height: 20} \
             QScrollBar::handle:horizontal {background: #ffee06; border: 2 solid #ffee06; border-radius: 4; margin: 2; min-width: 40} \
             QScrollBar::add-line:horizontal {width: 0} \
             QScrollBar::sub-line:horizontal {width: 0} \
             QScrollBar:vertical {background: #36362e; border: 0; border-radius: 4; margin: 0; width: 20} \
             QScrollBar::handle:vertical {background: #ffee06; border: 2 solid #ffee06; border-radius: 4; margin: 2; min-height: 40} \
             QScrollBar::add-line:vertical {height: 0} \
             QScrollBar::sub-line:vertical {height: 0} \
            ')
        switch.pressed.connect(self.switch_pressed)
        cancel.pressed.connect(self.cancel_pressed)
        fromFileButton.pressed.connect(self.from_pressed)
        if os.path.exists('{}/linuxcnc/configs'.format(os.path.expanduser('~'))):
            self.DIR = '{}/linuxcnc/configs'.format(os.path.expanduser('~'))
        elif os.path.exists('{}/linuxcnc'.format(os.path.expanduser('~'))):
            self.DIR = '{}/linuxcnc'.format(os.path.expanduser('~'))
        else:
            self.DIR = '{}'.format(os.path.expanduser('~'))
        self.fromFileName = None
        self.fromFilePath = None

# POPUP INFO DIALOG
    def dialog_ok(self, title, text):
        msgBox = QMessageBox()
        msgBox.setIcon(QMessageBox.Information)
        msgBox.setWindowTitle('{}'.format(title))
        msgBox.setText('{}'.format(text))
        msgBox.setStandardButtons(QMessageBox.Ok)
        buttonK = msgBox.button(QMessageBox.Ok)
        buttonK.setIcon(QIcon())
        buttonK.setText('OK')
        msgBox.setStyleSheet('* {color: #ffee06; background: #16160e; font: 12pt Lato} \
                             QPushButton {border: 1px solid #ffee06; border-radius: 4; height: 20}' \
                         )
        ret = msgBox.exec_()
        return ret

# SELECT QTPLASMAC INI FILE
    def from_pressed(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        name, _ = QFileDialog.getOpenFileName(
                    parent=self,
                    caption=self.tr("Select a ini file"),
                    filter=self.tr('INI files (*.ini);;INI files (*.[iI][nN][iI])'),
                    directory=self.DIR,
                    options=options
                    )
        if name:
            self.fromFile.setText(name)
            self.fromFileName = name
            self.fromFilePath = os.path.dirname(name)
        else:
            self.fromFile.setText('')
            self.fromFileName = None
            self.fromFilePath = None

# CLOSE PROGRAM
    def cancel_pressed(self):
        sys.exit()

# SWITCH
    def switch_pressed(self):
        if not self.fromFilePath:
            msg  = 'Missing path to QtPlasmaC configuration\n'
            self.dialog_ok('PATH ERROR', msg)
            return
        self.date = '{}-{:02d}-{:02d}'.format(time.localtime(time.time())[0], \
                                          time.localtime(time.time())[1], \
                                          time.localtime(time.time())[2],)
        self.time = '{:02d}-{:02d}-{:02d}'.format(time.localtime(time.time())[3], \
                                              time.localtime(time.time())[4], \
                                              time.localtime(time.time())[4],)
        linkName = os.path.join(self.fromFilePath, 'qtplasmac')
        if os.path.exists(linkName):
            try:
                if os.path.islink(linkName):
                    os.unlink(linkName)
                else:
                    os.rename(linkName, '{}_old_{}'.format(linkName, time.time()))
                os.symlink(self.commonPath, linkName)
            except:
                msg  = 'Could not switch this QtPlasmaC configuration\n'
                self.dialog_ok('SWITCH ERROR', msg)
                return
        else:
            msg  = 'This doesn\'t seem to be a valid QtPlasmaC configuration\n'
            self.dialog_ok('CONFIG ERROR', msg)
            return
        self.hide()
        msg  = 'QtPlasmaC installation type switched\n'
        self.dialog_ok('SWITCH SUCCESS', msg)
        sys.exit()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = ConfSwitch()
    w.show()
    sys.exit(app.exec_())
