
'''
qtplasmac-plasmac2qt.py

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

class Converter(QMainWindow, object):

# INITIALISATION
    def __init__(self, parent=None):
        super(Converter, self).__init__(parent)
        self.appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        if len(sys.argv) > 1:
            if os.path.isfile(sys.argv[1]):
                self.mode = 'auto'
                self.iniIn = sys.argv[1]
            else:
                print('{} is not a valid file.'.format(sys.argv[1]))
                sys.exit(0)
        else:
            self.mode = ''
            self.iniIn = ''
        if 'usr' in self.appPath:
            self.commonPath = '/usr/share/doc/linuxcnc/examples/sample-configs/by_machine/qtplasmac/qtplasmac'
            self.simPath = '/usr/share/doc/linuxcnc/examples/sample-configs/by_machine/qtplasmac'
        else:
            self.commonPath = self.appPath.replace('bin', 'configs/by_machine/qtplasmac/qtplasmac')
            self.simPath = self.appPath.replace('bin', 'configs/by_machine/qtplasmac')
        self.setFixedWidth(600)
        self.setFixedHeight(400)
        wid = QWidget(self)
        qtRectangle = self.frameGeometry()
        centerPoint = QDesktopWidget().availableGeometry().center()
        qtRectangle.moveCenter(centerPoint)
        self.move(qtRectangle.topLeft())
        self.setCentralWidget(wid)
        layout = QHBoxLayout()
        wid.setLayout(layout)
        self.setWindowTitle('PLASMAC2QT')
        vBox = QVBoxLayout()
        if self.mode == 'auto':
            heading  = 'Plasmac is not available in LinuxCNC V2.9 and later\n\n'
        else:
            heading = ''
        heading += 'Convert Existing PlasmaC Configuration To A New QtPlasmaC Configuration\n'
        headerLabel = QLabel(heading)
        headerLabel.setAlignment(Qt.AlignCenter)
        vBox.addWidget(headerLabel)
        vSpace01 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace01)
        fromLabel = QLabel('INI FILE IN EXISTING PLASMAC CONFIG:')
        fromLabel.setAlignment(Qt.AlignBottom)
        vBox.addWidget(fromLabel)
        fromFileHBox = QHBoxLayout()
        self.fromFile = QLineEdit()
        self.fromFile.setEnabled(False)
        if self.mode:
            self.fromFile.setText(self.iniIn)
        else:
            fromFileButton = QPushButton('SELECT')
            fromFileHBox.addWidget(fromFileButton)
        fromFileHBox.addWidget(self.fromFile)
        vBox.addLayout(fromFileHBox)
        vSpace02 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace02)
        aspectLabel = QLabel('MONITOR ASPECT RATIO:')
        vBox.addWidget(aspectLabel)
        aspectHBox = QHBoxLayout()
        self.aspectGroup = QButtonGroup()
        self.aspect0 = QRadioButton('16:9')
        self.aspect0.setFixedHeight(25)
        self.aspect0.setChecked(True)
        aspectHBox.addWidget(self.aspect0)
        self.aspectGroup.addButton(self.aspect0, 0)
        self.aspect1 = QRadioButton('9:16')
        aspectHBox.addWidget(self.aspect1)
        self.aspectGroup.addButton(self.aspect1, 1)
        self.aspect2 = QRadioButton('4:3')
        aspectHBox.addWidget(self.aspect2)
        self.aspectGroup.addButton(self.aspect2, 2)
        self.aspectGroup.buttonClicked.connect(self.aspect_group_clicked)
        vBox.addLayout(aspectHBox)
        vSpace03 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace03)
        self.estopLabel = QLabel('ESTOP IS AN INDICATOR ONLY')
        vBox.addWidget(self.estopLabel)
        estopHBox = QHBoxLayout()
        self.estopGroup = QButtonGroup()
        self.estop0 = QRadioButton('ESTOP: 0')
        self.estop0.setFixedHeight(25)
        self.estop0.setChecked(True)
        estopHBox.addWidget(self.estop0)
        self.estopGroup.addButton(self.estop0, 0)
        self.estop1 = QRadioButton('ESTOP: 1')
        estopHBox.addWidget(self.estop1)
        self.estopGroup.addButton(self.estop1, 1)
        self.estop2 = QRadioButton('ESTOP: 2')
        estopHBox.addWidget(self.estop2)
        self.estopGroup.addButton(self.estop2, 2)
        self.estopGroup.buttonClicked.connect(self.estop_group_clicked)
        vBox.addLayout(estopHBox)
        vSpace3 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace3)
        laserOnLabel = QLabel('OPTIONAL:\nLaser On HAL pin: (bit output)')
        vBox.addWidget(laserOnLabel)
        self.laserOnPin = QLineEdit()
        vBox.addWidget(self.laserOnPin)
        vSpace04 = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        vBox.addItem(vSpace04)
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
             QPushButton {border: 1 solid #ffee06; border-radius: 4; height: 24; width: 80; max-width: 90} \
             QFileDialog QPushButton {border: 1 solid #ffee06; border-radius: 4; height: 30; margin: 6} \
             QPushButton:pressed {color: #16160e; background: #ffee06} \
             QLineEdit {border: 1 solid #ffee06; border-radius: 4; height: 24; padding-left: 8} \
             QFileDialog QLineEdit {border: 1 solid #ffee06; border-radius: 4; height: 30} \
             QTableView::item:selected:active {color: #16160e; background-color: #ffee06} \
             QTableView::item:selected:!active {color: #16160e; background-color: #ffee06} \
             QHeaderView::section {color: #ffee06; background-color: #36362e; border: 1 solid #ffee06; border-radius: 4; margin: 2} \
             QComboBox {color: #ffee06; background-color: #16160e; border: 1 solid #ffee06; border-radius: 4; height: 30} \
             QComboBox::drop-down {width: 0} \
             QComboBox QListView {border: 4p solid #ffee06; border-radius: 0} \
             QComboBox QAbstractItemView {border: 2px solid #ffee06; border-radius: 4} \
             QScrollBar:horizontal {background: #36362e; border: 0; border-radius: 4; margin: 0; height: 20} \
             QScrollBar::handle:horizontal {background: #ffee06; border: 2 solid #ffee06; border-radius: 4; margin: 2; width: 40} \
             QScrollBar::add-line:horizontal {width: 0} \
             QScrollBar::sub-line:horizontal {width: 0} \
             QScrollBar:vertical {background: #36362e; border: 0; border-radius: 4; margin: 0; width: 20} \
             QScrollBar::handle:vertical {background: #ffee06; border: 2 solid #ffee06; border-radius: 4; margin: 2; height: 40} \
             QScrollBar::add-line:vertical {height: 0} \
             QScrollBar::sub-line:vertical {height: 0} \
             QRadioButton::indicator {border: 1px solid #ffee06; border-radius: 4; height: 20; width: 20} \
             QRadioButton::indicator:checked {background: #ffee06} \
            ')
        convert.pressed.connect(self.convert_pressed)
        cancel.pressed.connect(self.close_app)
        if not self.mode:
            fromFileButton.pressed.connect(self.from_pressed)
        if os.path.exists('{}/linuxcnc/configs'.format(os.path.expanduser('~'))):
            self.DIR = '{}/linuxcnc/configs'.format(os.path.expanduser('~'))
        elif os.path.exists('{}/linuxcnc'.format(os.path.expanduser('~'))):
            self.DIR = '{}/linuxcnc'.format(os.path.expanduser('~'))
        else:
            self.DIR = '{}'.format(os.path.expanduser('~'))
        self.display = 'DISPLAY                 = qtvcp qtplasmac\n'
        self.estop = 'ESTOP_TYPE              = 0\n'

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
        msgBox.setStyleSheet('QWidget {color: #ffee06; background: #16160e; font: 12pt Lato} \
                              QPushButton {border: 1px solid #ffee06; border-radius: 4; height: 20} \
                             ')
        msgBox.setBaseSize(QSize(800, 800))
        ret = msgBox.exec_()
        return ret

# SELECT PLASMAC INI FILE
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
        if name and os.path.isfile(name):
            self.fromFile.setText(name)
        else:
            self.fromFile.setText('')
        self.iniIn = self.fromFile.text()

# ASPECT CHANGED
    def aspect_group_clicked(self, button):
        if self.aspectGroup.id(button) == 0:
            self.display = 'DISPLAY                 = qtvcp qtplasmac\n'
        elif self.aspectGroup.id(button) == 1:
            self.display = 'DISPLAY                 = qtvcp qtplasmac_9x16\n'
        elif self.aspectGroup.id(button) == 2:
            self.display = 'DISPLAY                 = qtvcp qtplasmac_4x3\n'

# SET ESTOP DESCRIPTION
    def estop_group_clicked(self, button):
        if self.estopGroup.id(button) == 0:
            self.estopLabel.setText('ESTOP IS AN INDICATOR ONLY')
            self.estop = 'ESTOP_TYPE              = 0\n'
        elif self.estopGroup.id(button) == 1:
            self.estopLabel.setText('ESTOP IS HIDDEN')
            self.estop = 'ESTOP_TYPE              = 1\n'
        elif self.estopGroup.id(button) == 2:
            self.estopLabel.setText('ESTOP IS A BUTTON')
            self.estop = 'ESTOP_TYPE              = 2\n'

# CLOSE PROGRAM
    def close_app(self):
        sys.exit(2)

# CONVERT
    def convert_pressed(self):
    # CHECK IF INI FILE NAME EXISTS
        if not self.iniIn:
            return
    # CHECK IF FULL PATH EXISTS
        if not os.path.dirname(self.iniIn):
            msg  = 'Missing path to a PlasmaC configuration\n'
            self.dialog_ok('PATH ERROR', msg)
            self.fromFile.setFocus()
            return
    # CHECK IF VALID PLASMAC CONFIG
        if not os.path.exists('{}/plasmac'.format(os.path.dirname(self.iniIn))):
            msg  = '{}\n'.format(self.iniIn)
            msg += '\n is not a PlasmaC configuration\n'
            self.dialog_ok('CONFIG ERROR', msg)
            self.fromFile.setFocus()
            return
    # CHECK IF SIM CONFIG
        simConfig = False
        with open(self.iniIn, 'r') as inFile:
            for line in inFile:
                if 'plasmac_test.py' in line and not line.startswith('#'):
                    simConfig = True
                    break
    # SET FILENAMES AND PATHS
        fName = os.path.basename(self.iniIn)
        newDir = os.path.dirname(self.iniIn)
        oldDir = '{}_{}_{}'.format(os.path.dirname(self.iniIn), 'plasmac', str(time.time()).split('.')[0])
        newIniFile = os.path.join(newDir, fName)
        oldIniFile = os.path.join(oldDir, fName)
    # CREATE NEW DIRECTORY AND BACKUPS DIRECTORY
        try:
            os.rename(newDir, oldDir)
            os.makedirs('{}/backups'.format(newDir))
        except:
            msg  = 'Could not create directory\n'.format(newDir)
            self.dialog_ok('DIRECTORY ERROR', msg)
            return
    # GET THE MACHINE NAME
        with open(oldIniFile) as inFile:
            while(1):
                line = inFile.readline()
                if not line:
                    print('cannot find [EMC] section in ini file')
                    return
                if line.startswith('[EMC]'):
                    break
            while(1):
                line = inFile.readline()
                if not line:
                    print('cannot find MACHINE variable in ini file')
                    return
                if line.startswith('MACHINE'):
                    machineName = line.split('=')[1].strip().lower()
                    break
    # COPY ORIGINAL BASE MACHINE FILES IF EXISTING
        try:
            for filename in os.listdir('{}/backups'.format(oldDir)):
                if filename.startswith('_original'):
                    COPY('{}/backups/{}'.format(oldDir, filename), '{}/backups/{}'.format(newDir, filename))
        except:
            pass
    # CREATE LINK TO QTPLASMAC COMMON FILES
        try:
            os.symlink(self.commonPath , '{}/qtplasmac'.format(newDir))
        except:
            msg  = 'Could not link to Common directory: '
            msg += '{}\n'.format(self.commonPath)
            msg += '\nConversion cannot continue'
            self.dialog_ok('LINK ERROR', msg)
            self.fromFile.setFocus()
            return
    # COPY HAL FILES
        halFiles = []
        oldPostguiFile = None
        newPostguiFile = None
        newConnectionsFile = None
        with open(oldIniFile, 'r') as inFile:
            while(1):
                line = inFile.readline()
                if line.startswith('[HAL]'):
                    break
                if not line:
                    msg  = 'Could not get [HAL] section of ini file\n'
                    msg += '\nConversion cannot continue'
                    self.dialog_ok('INI FILE ERROR', msg)
                    self.fromFile.setFocus()
                    return
            while(1):
                line = inFile.readline()
                if line.startswith('POSTGUI_HALFILE'):
                    oldPostguiFile = line.split('=')[1].strip()
                    newPostguiFile = 'custom_postgui.hal'
                    halFiles.append(oldPostguiFile)
                    COPY('{}/{}'.format(oldDir, oldPostguiFile), '{}/{}'.format(newDir, newPostguiFile))
                elif 'connections.hal' in line:
                    oldConnectionsFile = line.split('=')[1].strip()
                    newConnectionsFile = 'custom.hal'
                    halFiles.append(newConnectionsFile)
                    COPY('{}/{}'.format(oldDir, oldConnectionsFile), '{}/{}'.format(newDir, newConnectionsFile))
                    with open('{}/{}'.format(newDir, newConnectionsFile), 'w') as outConFile:
                        with open('{}/{}'.format(oldDir, oldConnectionsFile), 'r') as inConFile:
                            for line in inConFile:
                                if 'plasmac:torch-on' in line:
                                    outConFile.write(line)
                                    outConFile.write('\n#***** LASER ALIGNMENT CONNECTION *****\n')
                                    if self.laserOnPin.text():
                                        outConFile.write('net plasmac:laser-on => {}\n'.format(self.laserOnPin.text()))
                                    else:
                                        outConFile.write('# net plasmac:laser-on => {YOUR_LASER_ON_OUTPUT}\n\n')
                                else:
                                    outConFile.write(line)
                elif line.startswith('HALFILE'):
                    if 'plasmac.tcl' in line:
                        halFiles.append('plasmac.tcl')
                    else:
                        hFile = line.split('=')[1].strip()
                        with open('{}/{}'.format(oldDir, hFile), 'r') as inHal:
                            if simConfig and 'motor-pos-cmd' in inHal.read():
                                halFiles.append('machine.tcl')
                                COPY('{}/machine.tcl'.format(self.simPath), '{}/machine.tcl'.format(newDir))
                            else:
                                halFiles.append(hFile)
                                COPY('{}/{}'.format(oldDir, hFile), '{}/{}'.format(newDir, hFile))
                if not line or line.startswith('['):
                    break
    # COPY TOOL TABLE
        if os.path.exists('{}/tool.tbl'.format(oldDir)):
            COPY('{}/tool.tbl'.format(oldDir), '{}/tool.tbl'.format(newDir))
    # PARSE ORIGINAL INI FILE TO FIND BUTTONS
        self.buttons = {}
        for n in range(1, 20):
            self.buttons[n] = None
        numButton = 1
        n0,n1,name,code = '','','',''
        with open(oldIniFile, 'r') as inFile:
            while(1):
                line = inFile.readline()
                if line.startswith('[PLASMAC]'):
                    break
            while(1):
                line = inFile.readline()
                if line.startswith('['):
                    break
                if line.startswith('BUTTON_') and '_NAME' in line:
                    n0 = line.split('=')[0].strip().replace('BUTTON_','').replace('_NAME','')
                    name = line.split('=')[1].strip()
                if line.startswith('BUTTON_') and '_CODE' in line:
                    n1 = line.split('=')[0].strip().replace('BUTTON_','').replace('_CODE','')
                    code = line.split('=')[1].strip()
                if n0 == n1 and name and code:
                    self.buttons[numButton] = [name, code]
                    n0,n1,name,code = '','','',''
                    numButton += 1
    # MAKE NEW INI FILE
        section = ''
        with open(newIniFile, 'w') as outFile:
            with open(oldIniFile, 'r') as inFile:
                for line in inFile:
                # SET SECTION NAMES
                    if line.startswith('['):
                        section = ''
                    if line.startswith('[APPLICATIONS]'):
                        section = 'APPLICATIONS'
                    if line.startswith('[PLASMAC]'):
                        section = 'PLASMAC'
                        line = '[QTPLASMAC]\n'
                    if line.startswith('[FILTER]'):
                        section = 'FILTER'
                    if line.startswith('[RS274NGC]'):
                        section = 'RS274NGC'
                    if line.startswith('[HAL]'):
                        section = 'HAL'
                    if line.startswith('[DISPLAY]'):
                        section = 'DISPLAY'
                    if line.startswith('[EMC]'):
                        section = 'EMC'
                # CONVERT SECTION PARAMETERS
                    if section == 'APPLICATIONS':
                        continue
                    if section == 'PLASMAC':
                        omissions = ['LAST','CONF','FONT','MAXI','WIND','THEM','AXIS','CONE','BUTT','PAUS','TORC']
                        if line.startswith('#') or line.startswith('PM_PR'):
                            continue
                        if line[:4] in omissions:
                            continue
                        if line.startswith('['):
                            outFile.write('\n')
                        if line.strip():
                            if line.startswith('MODE'):
                                outFile.write(line)
                                outFile.write(self.estop)
                                outFile.write('#LASER_TOUCHOFF          = X0.0 Y0.0\n')
                                outFile.write('#CAMERA_TOUCHOFF         = X0.0 Y0.0\n')
                                for n in range(1, 20):
                                    if self.buttons[n]:
                                        outFile.write('BUTTON_{}_NAME           = {}\n'.format(n, self.buttons[n][0]))
                                        outFile.write('BUTTON_{}_CODE           = {}\n'.format(n, self.buttons[n][1]))
                            else:
                                outFile.write(line)
                        continue
                    elif section == 'FILTER':
                        if line.startswith('[FILTER]'):
                            outFile.write('\n{}'.format(line))
                            outFile.write('PROGRAM_EXTENSION       = .ngc,.nc,.tap GCode File (*.ngc, *.nc, *.tap)\n')
                            outFile.write('ngc                     = ./qtplasmac/qtplasmac_gcode.py\n')
                            outFile.write('nc                      = ./qtplasmac/qtplasmac_gcode.py\n')
                            outFile.write('tap                     = ./qtplasmac/qtplasmac_gcode.py\n')
                        continue
                    elif section == 'RS274NGC':
                        if line.startswith('SUBROUTINE') or line.startswith('USER_M_PATH'):
                            if 'plasmac' in line:
                                line = line.replace('plasmac', 'qtplasmac')
                            else:
                                line = '{}:./qtplasmac\n'.format(line.strip())
                        if line.startswith('#') or line.replace(' ', '').strip() == 'FEATURES=12':
                            continue
                        if line.startswith('['):
                            outFile.write('\n')
                        if line.strip():
                            outFile.write(line)
                        continue
                    elif section == 'HAL':
                        if line.startswith('[HAL]'):
                            outFile.write('\n{}'.format(line))
                            outFile.write('TWOPASS                 = ON\n')
                            outFile.write('HALUI                   = halui\n')
                            for file in halFiles:
                                if '_connections' in file:
                                    outFile.write('HALFILE                 = custom.hal\n')
                                elif 'postgui' in file:
                                    outFile.write('POSTGUI_HALFILE         = custom_postgui.hal\n')
                                else:
                                    outFile.write('HALFILE                 = {}\n'.format(file))
                            if simConfig:
                                outFile.write('POSTGUI_HALFILE         = sim_postgui.tcl\n')
                        continue
                    elif section == 'DISPLAY':
                        if line.startswith('DISPLAY'):
                            line = self.display
                        omissions = ['OPEN_F','EDITOR','TOOL_E','USER_C','EMBED_','GLADEV','CONE_B']
                        if line.startswith('#'):
                            continue
                        if line[:6] in omissions:
                            continue
                        if line.startswith('['):
                            outFile.write('\n')
                        if line.strip():
                            outFile.write(line)
                        continue
                    elif section == 'EMC':
                        if 'enable the axis_tweaks' in line:
                            continue
                        if line.startswith('['):
                            outFile.write('\n')
                        if line.strip():
                            outFile.write(line)
                        continue
                # NO CONVERSION REQUIRED
                    else:
                        if line.strip():
                            if line.startswith('['):
                                outFile.write('\n')
                            if 'marry this config' in line or 'sim testing panel' in line:
                                continue
                            outFile.write(line)
        self.prefParms = []
        if os.path.isfile(os.path.join(oldDir, machineName + '_config.cfg')):
            self.read_con_file(os.path.join(oldDir, machineName + '_config.cfg'))
        else:
            print('file not found, config parameters can not be converted.')
        if os.path.isfile(os.path.join(oldDir, machineName + '_run.cfg')):
            self.read_run_file(os.path.join(oldDir, machineName + '_run.cfg'))
        else:
            print('file not found, run parameters can not be converted.')
        if os.path.isfile(os.path.join(oldDir, machineName + '_wizards.cfg')):
            self.read_wiz_file(os.path.join(oldDir, machineName + '_wizards.cfg'))
        else:
            print('file not found, wizard parameters can not be converted.')
        if os.path.isfile(os.path.join(oldDir, 'plasmac_stats.var')):
            self.read_sta_file(os.path.join(oldDir, 'plasmac_stats.var'))
        else:
            print('file not found, statistics can not be converted.')
        if os.path.isfile(os.path.join(oldDir, machineName + '_material.cfg')):
            self.read_mat_file(os.path.join(oldDir, machineName + '_material.cfg'), newDir, machineName)
        else:
            print('file not found, materials can not be converted.')
        self.write_prefs_file(newDir, machineName)
    # ADD A SIM POSTGUI IF A SIM CONFIG
        if simConfig:
            with open('{}/{}'.format(newDir, 'sim_postgui.tcl'), 'a') as outFile:
                outFile.write(self.sim_postgui())
    # WE GOT THIS FAR SO IT MAY HAVE WORKED
        msg  = 'Conversion appears successful.\n'
        if self.mode == 'automatic':
            msg += '\nRestart LinuxCNC using the following ini file:\n'
        else:
            msg += '\nStart LinuxCNC using the following ini file:\n'
        msg += '\n{}/{}.ini\n'.format(newDir, machineName)
        self.dialog_ok('SUCCESS', msg)
        print(msg)
        sys.exit(0)


# READ THE ORIGINAL <MACHINE>_CONFIG.CFG FILE
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

# READ THE ORIGINAL <MACHINE>_RUN.CFG FILE
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

# READ THE ORIGINAL <MACHINE>_WIZARDS.CFG FILE
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

# READ THE ORIGINAL PLASMAC_STATS.VAR FILE
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

# READ THE ORIGINAL <MACHINE>_MATERIAL.CFG FILE
    def read_mat_file(self, matFile, newDir, machineName):
        newFile = '{}/{}_material.cfg'.format(newDir, machineName)
        if os.path.isfile(newFile):
            matCopy = '{}_{}'.format(newFile, self.date, str(time.time()).split('.')[0])
            COPY(newFile, matCopy)
        with open(newFile, 'w') as outFile:
            with open(matFile) as inFile:
                for line in inFile:
                    if line.startswith('THC') or line.startswith('#THC'):
                        continue
                    outFile.write(line)

# WRITE THE NEW QTPLASMAC.PREFS FILE
    def write_prefs_file(self, newDir, machineName):
        prefsFile = '{}/qtplasmac.prefs'.format(newDir)
        with open(prefsFile, 'w') as outFile:
            outFile.write(\
                '[NOTIFY_OPTIONS]\n' \
                'notify_start_greeting = False\n' \
                'notify_start_title = Welcome To QtPlasmaC\n' \
                'notify_start_detail = This option can be changed in {}\n' \
                'notify_start_timeout = 5\n\n' \
                .format(prefsFile))
            for item in self.prefParms:
                outFile.write('{}\n'.format(item))

# SIM CONFIG POSTGUI EXTRAS
    def sim_postgui(self):
        sim  = '\n# SIMULATOR PANEL CONNECTIONS\n'
        sim += 'loadusr -Wn qtplasmac_sim qtvcp qtplasmac_sim.ui\n'
        sim += 'net plasmac:torch-on                                    =>  qtplasmac_sim.torch_on\n'
        sim += 'net sim:arc-voltage-in  qtplasmac_sim.arc_voltage_out-f =>  plasmac.arc-voltage-in  qtplasmac_sim.arc_voltage_in\n'
        sim += 'net sim:move-up         qtplasmac_sim.move_up           =>  plasmac.move-up\n'
        sim += 'net sim:move-down       qtplasmac_sim.move_down         =>  plasmac.move-down\n'
        sim += '# if no new dbounce then use old debounce component for legacy plasmac conversions\n'
        sim += 'if {[hal list pin db_float.out] != {}} {\n'
        sim += 'net sim:arc-ok          qtplasmac_sim.arc_ok            =>  db_arc-ok.in\n'
        sim += 'net sim:breakaway       qtplasmac_sim.sensor_breakaway  =>  db_breakaway.in\n'
        sim += 'net sim:float           qtplasmac_sim.sensor_float      =>  db_float.in\n'
        sim += 'net sim:ohmic           qtplasmac_sim.sensor_ohmic      =>  db_ohmic.in\n'
        sim += '} else {\n'
        sim += '    puts "using old debounce component"\n'
        sim += '    puts "it is recommended to convert to the new dbounce component\n"\n'
        sim += 'net sim:breakaway       qtplasmac_sim.sensor_breakaway  =>  debounce.0.1.in\n'
        sim += 'net sim:float           qtplasmac_sim.sensor_float      =>  debounce.0.0.in\n'
        sim += 'net sim:ohmic           qtplasmac_sim.sensor_ohmic      =>  debounce.0.2.in\n'
        sim += '}\n\n'
        return sim

if __name__ == '__main__':
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)
    w = Converter()
    app.lastWindowClosed.connect(w.close_app)
    w.show()
    sys.exit(app.exec_())
