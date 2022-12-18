
'''
qtplasmac-cfg2prefs.py

This file is used to convert settings in the .cfg files from a PlasmaC
configuration to the .prefs file for a QtPlasmaC configuration.

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

class Cfg2Prefs(QMainWindow, object):

# INITIALISATION
    def __init__(self, parent=None):
        super(Cfg2Prefs, self).__init__(parent)
        self.setFixedWidth(600)
        self.setFixedHeight(340)
        wid = QWidget(self)
        qtRectangle = self.frameGeometry()
        centerPoint = QDesktopWidget().availableGeometry().center()
        qtRectangle.moveCenter(centerPoint)
        self.move(qtRectangle.topLeft())
        self.setCentralWidget(wid)
        layout = QHBoxLayout()
        wid.setLayout(layout)
        iconPath = 'share/icons/hicolor/scalable/apps/linuxcnc_alt/linuxcncicon_plasma.svg'
        appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        iconBase = '/usr' if appPath == '/usr/bin' else appPath.replace('/bin', '/debian/extras/usr')
        self.setWindowIcon(QIcon(os.path.join(iconBase, iconPath)))
        self.setWindowTitle('QtPlasmaC Cfg2Prefs')
        vBox = QVBoxLayout()
        heading  = 'Convert Existing PlasmaC .cfg Files To A New QtPlasmaC .prefs File\n'
        headerLabel = QLabel(heading)
        headerLabel.setAlignment(Qt.AlignCenter)
        vBox.addWidget(headerLabel)
        vSpace1 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace1)
        fromLabel = QLabel('INI file in existing PlasmaC config:')
        fromLabel.setAlignment(Qt.AlignBottom)
        vBox.addWidget(fromLabel)
        fromFileHBox = QHBoxLayout()
        fromFileButton = QPushButton('SELECT')
        self.fromFile = QLineEdit()
        self.fromFile.setEnabled(False)
        fromFileHBox.addWidget(fromFileButton)
        fromFileHBox.addWidget(self.fromFile)
        vBox.addLayout(fromFileHBox)
        vSpace2 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace2)
        toLabel = QLabel('INI file in new QtPlasmaC config:')
        toLabel.setAlignment(Qt.AlignBottom)
        vBox.addWidget(toLabel)
        toFileHBox = QHBoxLayout()
        toFileButton = QPushButton('SELECT')
        self.toFile = QLineEdit()
        self.toFile.setEnabled(False)
        toFileHBox.addWidget(toFileButton)
        toFileHBox.addWidget(self.toFile)
        vBox.addLayout(toFileHBox)
        vSpace3 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace3)
        buttonHBox = QHBoxLayout()
        convert = QPushButton('CONVERT')
        buttonHBox.addWidget(convert)
        cancel = QPushButton('EXIT')
        buttonHBox.addWidget(cancel)
        vBox.addLayout(buttonHBox)
        layout.addLayout(vBox)
        self.setStyleSheet( \
            'QWidget {color: #ffee06; background: #16160e} \
            QLabel {height: 20} \
            QPushButton {border: 1 solid #ffee06; border-radius: 4; height: 40; width: 80; max-width: 90} \
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
        convert.pressed.connect(self.convert_pressed)
        cancel.pressed.connect(self.cancel_pressed)
        fromFileButton.pressed.connect(self.from_pressed)
        toFileButton.pressed.connect(self.to_pressed)
        if os.path.exists('{}/linuxcnc/configs'.format(os.path.expanduser('~'))):
            self.DIR = '{}/linuxcnc/configs'.format(os.path.expanduser('~'))
        elif os.path.exists('{}/linuxcnc'.format(os.path.expanduser('~'))):
            self.DIR = '{}/linuxcnc'.format(os.path.expanduser('~'))
        else:
            self.DIR = '{}'.format(os.path.expanduser('~'))
        self.fromFileName = None
        self.fromFilePath = None
        self.toFileName = None
        self.toFilePath = None

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
        msgBox.setStyleSheet('* {color: #ffee06; background: #16160e; font: 12pt DejaVuSans} \
                             QPushButton {border: 1px solid #ffee06; border-radius: 4; height: 20}' \
                         )
        ret = msgBox.exec_()
        return ret

# SELECT PLASMAC INI FILE
    def from_pressed(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        name, _ = QFileDialog.getOpenFileName(
                    parent=self,
                    caption=self.tr("Select an INI file"),
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

# SELECT QTPLASMAC INI FILE
    def to_pressed(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        name, _ = QFileDialog.getOpenFileName(
                    parent=self,
                    caption=self.tr("Select an INI file"),
                    filter=self.tr('INI files (*.ini);;INI files (*.[iI][nN][iI])'),
                    directory=self.DIR,
                    options=options

                    )
        if name:
            self.toFile.setText(name)
            self.toFileName = name
            self.toFilePath = os.path.dirname(name)
        else:
            self.toFile.setText('')
            self.toFileName = None
            self.toFilePath = None

# CLOSE PROGRAM
    def cancel_pressed(self):
        sys.exit()

# CONVERT
    def convert_pressed(self):
        if not self.fromFilePath:
            msg  = 'Missing path to PlasmaC configuration\n'
            self.dialog_ok('Path Error', msg)
            return
        if not self.toFilePath:
            msg  = 'Missing path to QtPlasmaC configuration\n'
            self.dialog_ok('Path Error', msg)
            return
        if self.toFilePath == self.fromFilePath:
            msg  = 'Cannot operate on one folder\n'
            self.dialog_ok('Folder Error', msg)
            return
        self.prefParms = []
        self.date = '{}-{:02d}-{:02d}'.format(time.localtime(time.time())[0], \
                                          time.localtime(time.time())[1], \
                                          time.localtime(time.time())[2],)
        self.time = '{:02d}-{:02d}-{:02d}'.format(time.localtime(time.time())[3], \
                                              time.localtime(time.time())[4], \
                                              time.localtime(time.time())[4],)
        with open(self.fromFileName) as inFile:
            while(1):
                line = inFile.readline()
                if not line:
                    print('cannot find [EMC] section in INI file')
                    return
                if line.startswith('[EMC]'):
                    break
            while(1):
                line = inFile.readline()
                if not line:
                    print('cannot find MACHINE variable in INI file')
                    return
                if line.startswith('MACHINE'):
                    self.fromMachine = line.split('=')[1].strip().lower()
                    break
        with open(self.toFileName) as inFile:
            while(1):
                line = inFile.readline()
                if not line:
                    print('cannot find [EMC] section in INI file')
                    return
                if line.startswith('[EMC]'):
                    break
            while(1):
                line = inFile.readline()
                if not line:
                    print('cannot find MACHINE variable in INI file')
                    return
                if line.startswith('MACHINE'):
                    self.toMachine = line.split('=')[1].strip().lower()
                    break
        if os.path.isfile(os.path.join(self.fromFilePath, self.fromMachine + '_config.cfg')):
            self.read_con_file(os.path.join(self.fromFilePath, self.fromMachine + '_config.cfg'))
        else:
            print('file not found, config parameters can not be converted.')
        if os.path.isfile(os.path.join(self.fromFilePath, self.fromMachine + '_run.cfg')):
            self.read_run_file(os.path.join(self.fromFilePath, self.fromMachine + '_run.cfg'))
        else:
            print('file not found, run parameters can not be converted.')
        if os.path.isfile(os.path.join(self.fromFilePath, self.fromMachine + '_wizards.cfg')):
            self.read_wiz_file(os.path.join(self.fromFilePath, self.fromMachine + '_wizards.cfg'))
        else:
            print('file not found, wizard parameters can not be converted.')
        if os.path.isfile(os.path.join(self.fromFilePath, 'plasmac_stats.var')):
            self.read_sta_file(os.path.join(self.fromFilePath, 'plasmac_stats.var'))
        else:
            print('file not found, statistics can not be converted.')
        if os.path.isfile(os.path.join(self.fromFilePath, self.fromMachine + '_material.cfg')):
            self.read_mat_file(os.path.join(self.fromFilePath, self.fromMachine + '_material.cfg'))
        else:
            print('file not found, materials can not be converted.')
        self.write_prefs_file()

# READ THE <MACHINE>_CONFIG.CFG FILE
    def read_con_file(self, conFile):
        self.prefParms.append('[PLASMA_PARAMETERS]')
        with open(conFile) as inFile:
            for line in inFile:
                if line.startswith('setup-feed-rate'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Setup Feed Rate = {}'.format(value))
                elif line.startswith('arc-fail-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc Fail Timeout = {}'.format(value))
                elif line.startswith('arc-ok-high'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc OK High = {}'.format(value))
                elif line.startswith('arc-ok-low'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc OK Low = {}'.format(value))
                elif line.startswith('arc-max-starts'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc Maximum Starts = {}'.format(value))
                elif line.startswith('arc-voltage-offset'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc Voltage Offset = {}'.format(value))
                elif line.startswith('arc-voltage-scale'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc Voltage Scale = {}'.format(value))
                elif line.startswith('cornerlock-threshold'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Velocity Anti Dive Threshold = {}'.format(value))
                elif line.startswith('float-switch-travel'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Float Switch Travel = {}'.format(value))
                elif line.startswith('height-per-volt'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Height Per Volt = {}'.format(value))
                elif line.startswith('kerfcross-override'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Void Sense Override = {}'.format(value))
                elif line.startswith('ohmic-max-attempts'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Ohmic Maximum Attempts = {}'.format(value))
                elif line.startswith('ohmic-probe-offset'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Ohmic Probe Offset = {}'.format(value))
                elif line.startswith('pid-p-gain'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pid P Gain = {}'.format(value))
                elif line.startswith('pid-d-gain'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pid D Gain = {}'.format(value))
                elif line.startswith('pid-i-gain'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pid I Gain = {}'.format(value))
                elif line.startswith('probe-feed-rate'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Probe Feed Rate = {}'.format(value))
                elif line.startswith('probe-start-height'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Probe Start Height = {}'.format(value))
                elif line.startswith('arc-restart-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Arc Restart Delay = {}'.format(value))
                elif line.startswith('safe-height'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Safe Height = {}'.format(value))
                elif line.startswith('scribe-arm-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Scribe Arming Delay = {}'.format(value))
                elif line.startswith('scribe-on-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Scribe On Delay = {}'.format(value))
                elif line.startswith('skip-ihs-distance'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Skip IHS Distance = {}'.format(value))
                elif line.startswith('spotting-threshold'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Spotting Threshold = {}'.format(value))
                elif line.startswith('spotting-time'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Spotting Time = {}'.format(value))
                elif line.startswith('thc-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('THC Delay = {}'.format(value))
                elif line.startswith('thc-threshold'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('THC Threshold = {}'.format(value))
            self.prefParms.append('')

# READ THE <MACHINE>_RUN.CFG FILE
    def read_run_file(self, runFile):
        self.prefParms.append('[ENABLE_OPTIONS]')
        with open(runFile) as inFile:
            for line in inFile:
                if line.startswith('thc-enable'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('THC enable = {}'.format(value))
                elif line.startswith('cornerlock-enable'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Corner lock enable = {}'.format(value))
                elif line.startswith('kerfcross-enable'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Kerf cross enable = {}'.format(value))
                elif line.startswith('use-auto-volts'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Use auto volts = {}'.format(value))
                elif line.startswith('ohmic-probe-enable'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Ohmic probe enable = {}'.format(value))
            self.prefParms.append('')
        self.prefParms.append('[DEFAULT MATERIAL]')
        with open(runFile) as inFile:
            for line in inFile:
                if line.startswith('kerf-width'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Kerf width = {}'.format(value))
                elif line.startswith('pierce-height'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pierce height = {}'.format(value))
                elif line.startswith('pierce-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pierce delay = {}'.format(value))
                elif line.startswith('puddle-jump-height'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Puddle jump height = {}'.format(value))
                elif line.startswith('puddle-jump-delay'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Puddle jump delay = {}'.format(value))
                elif line.startswith('cut-height'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut height = {}'.format(value))
                elif line.startswith('cut-feed-rate'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut feed rate = {}'.format(value))
                elif line.startswith('cut-amps'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut amps = {}'.format(value))
                elif line.startswith('cut-volts'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut volts = {}'.format(value))
                elif line.startswith('pause-at-end'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pause at end = {}'.format(value))
                elif line.startswith('gas-pressure'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Gas pressure = {}'.format(value))
                elif line.startswith('cut-mode'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut mode = {}'.format(value))
            self.prefParms.append('')
        self.prefParms.append('[SINGLE CUT]')
        with open(runFile) as inFile:
            for line in inFile:
                if line.startswith('x-single-cut'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('X length = {}'.format(value))
                elif line.startswith('y-single-cut'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Y length = {}'.format(value))
            self.prefParms.append('')

# READ THE <MACHINE>_WIZARDS.CFG FILE
    def read_wiz_file(self, wizFile):
        self.prefParms.append('[CONVERSATIONAL]')
        with open(wizFile) as inFile:
            for line in inFile:
                if line.startswith('preamble'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Preamble = {}'.format(value))
                elif line.startswith('origin'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Origin = {}'.format(value))
                elif line.startswith('lead-in'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Leadin = {}'.format(value))
                elif line.startswith('lead-out'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Leadout = {}'.format(value))
                elif line.startswith('hole-diameter'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Hole diameter = {}'.format(value))
                elif line.startswith('hole-speed'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Hole speed = {}'.format(value))
                elif line.startswith('grid-size'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Grid Size = {}'.format(value))
            self.prefParms.append('')

# READ THE PLASMAC_STATS.VAR FILE
    def read_sta_file(self, staFile):
        self.prefParms.append('[STATISTICS]')
        with open(staFile) as inFile:
            for line in inFile:
                if line.strip().startswith('PIERCE_COUNT'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Pierce count = {}'.format(value))
                elif line.strip().startswith('CUT_LENGTH'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut length = {}'.format(value))
                elif line.strip().startswith('CUT_TIME'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Cut time = {}'.format(value))
                elif line.strip().startswith('TORCH_TIME'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Torch on time = {}'.format(value))
                elif line.strip().startswith('RUN_TIME'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Program run time = {}'.format(value))
                elif line.strip().startswith('RAPID_TIME'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Rapid time = {}'.format(value))
                elif line.strip().startswith('PROBE_TIME'):
                    value = line.split('=')[1].strip()
                    self.prefParms.append('Probe time = {}'.format(value))
            self.prefParms.append('')

# READ THE <MACHINE>_MATERIAL.CFG FILE
    def read_mat_file(self, matFile):
        newFile = os.path.join(self.toFilePath, self.toMachine + '_material.cfg')
        if os.path.isfile(newFile):
            matCopy = '{}_{}_{}'.format(newFile, self.date, self.time)
            COPY(newFile, matCopy)
        with open(newFile, 'w') as outFile:
            with open(matFile) as inFile:
                for line in inFile:
                    if line.startswith('THC') or line.startswith('#THC'):
                        continue
                    outFile.write(line)

# WRITE THE QTPLASMAC.PREFS FILE
    def write_prefs_file(self):
        inPrefs = []
        BYPASS = ['[PLASMA_PARAMETERS]', '[ENABLE_OPTIONS]', \
                  '[DEFAULT MATERIAL]', '[SINGLE CUT]', \
                  '[CONVERSATIONAL]', '[STATISTICS]']
        bypass = False
        prefsFile = os.path.join(self.toFilePath, self.toMachine + '.prefs')
        prefsCopy = '{}_{}_{}'.format(prefsFile, self.date, self.time)
        COPY(prefsFile, prefsCopy)
        with open(prefsFile, 'w') as outFile:
            with open(prefsCopy, 'r') as inFile:
                for line in inFile:
                    if line.startswith('[') and not line.strip() in BYPASS:
                        bypass = False
                    elif line.startswith('[') and line.strip() in BYPASS:
                        bypass = True
                    if not bypass:
                        inPrefs.append(line)
                        outFile.write('{}'.format(line))
            for item in self.prefParms:
                outFile.write('{}\n'.format(item))
        msg  = 'Conversion appears successful.\n'
        msg += '\nAny time stamped backup of an original qtplasmac.prefs '
        msg += 'file or an original material.cfg file may be deleted from '
        msg += 'the configuration folder when the result of the conversion '
        msg += 'has been confirmed.\n'
        self.dialog_ok('Success', msg)
        sys.exit()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = Cfg2Prefs()
    w.show()
    sys.exit(app.exec_())
