#!/usr/bin/python

'''
qtplasmac-setup.py

This file is used to install a QtPlasmaC configuration.

Copyright (C) 2019 2020 Phillip A Carter

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
import linuxcnc
import time
from shutil import copy as COPY
from shutil import move as MOVE
from PyQt5.QtCore import * 
from PyQt5.QtWidgets import * 
from PyQt5.QtGui import * 

class Configurator(QMainWindow, object):

# INITIALISATION
    def __init__(self, parent=None):
        super(Configurator, self).__init__(parent)
        self.appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        if 'usr' in self.appPath:
            self.commonPath = '/usr/share/doc/linuxcnc/examples/sample-configs/by_machine/qtplasmac/qtplasmac/'
        else:
            pass
            self.commonPath = self.appPath.replace('bin', 'configs/by_machine/qtplasmac/qtplasmac/') 
        wid = QWidget(self)
        self.setCentralWidget(wid)
        self.layout = QHBoxLayout()  
        wid.setLayout(self.layout)
        self.setWindowTitle('QtPlasmaC Setup')
        self.setFixedWidth(290)
        self.setFixedHeight(120)
        self.new = QPushButton('New')
        self.rec = QPushButton('Reconfigure')
        self.can = QPushButton('Exit')
        self.layout.addWidget(self.new, 0)
        self.layout.addWidget(self.rec, 1)
        self.layout.addWidget(self.can, 2)
        self.configPath = os.path.expanduser('~') + '/linuxcnc/configs'
        self.new.pressed.connect(lambda:self.on_selection('new'))
        self.rec.pressed.connect(lambda:self.on_selection('reconfigure'))
        self.can.pressed.connect(lambda:self.on_selection('cancel'))

    def on_selection(self, selection):
        width = self.frameGeometry().width()
        height = self.frameGeometry().height()
        self.configureType = selection
        if self.configureType == 'new':
            msg  = '\nBefore using this configurator you should already have a\n'
            msg += 'working configuration for your base machine.\n\n'
            msg += 'The base machine should be fully operational.\n\n'
            msg += 'If you don\'t have a working configuration then you need\n'
            msg += 'to exit the configurator and create one.\n\n'
            result = self.dialog_ok_cancel('New Config Prerequisites', msg, 'Continue','Cancel')
            if result != QMessageBox.Yes:
                return
        elif self.configureType == 'reconfigure':
            msg  = '\nThis configurator will enable the modifying\n'
            msg += 'of an existing configuration.\n\n'
            msg += 'If you do not have an existing configuration then\n'
            msg += 'you need to exit the configurator and create one.\n\n'
            result = self.dialog_ok_cancel( 'Reconfigure Prerequisites', msg, 'Continue','Cancel')
            if result != QMessageBox.Yes:
                return
        else:
            sys.exit()
        for widget in [self.can, self.rec, self.new]:
            self.layout.removeWidget(widget)
            widget.setParent(None)
        self.create_widgets()
        self.iniFileButton.pressed.connect(self.ini_file_select_pressed)
        self.create.clicked.connect(self.on_create_clicked)
        self.cancel.clicked.connect(self.cancel_clicked)
        self.orgIniFile = ''
        self.configDir = ''
        if self.configureType == 'new':
            self.halFileButton.pressed.connect(self.hal_file_select_pressed)
            self.nameFile.textChanged.connect(self.machine_name_changed)
        if self.configureType == 'new' or self.configureType == 'reconfigure':
            self.aspectGroup.buttonClicked.connect(self.aspect_group_clicked)
            self.modeGroup.buttonClicked.connect(self.mode_group_clicked)
            self.estopGroup.buttonClicked.connect(self.estop_group_clicked)
            self.aspect = ''
            self.mode = 0
            self.estop = 0
            self.newIniFile = ''
            self.orgHalFile = ''
            self.inPlace = False
            self.set_mode()

# OK/CANCEL POPUP DIALOG
    def dialog_ok_cancel(self, title, text, name1, name2):
        msgBox = QMessageBox()
        msgBox.setIcon(QMessageBox.Warning)
        msgBox.setWindowTitle('{}'.format(title))
        msgBox.setInformativeText('{}'.format(text))
        msgBox.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        buttonY = msgBox.button(QMessageBox.Yes)
        buttonY.setIcon(QIcon())
        buttonY.setText(name1)
        buttonN = msgBox.button(QMessageBox.No)
        buttonN.setIcon(QIcon())
        buttonN.setText(name2)
        msgBox.setDefaultButton(QMessageBox.No)
        ret = msgBox.exec_()
        return ret

# OK POPUP DIALOG
    def dialog_ok(self, title, text):
        msgBox = QMessageBox()
        msgBox.setIcon(QMessageBox.Information)
        msgBox.setWindowTitle('{}'.format(title))
        msgBox.setInformativeText('{}'.format(text))
        msgBox.setStandardButtons(QMessageBox.Ok)
        buttonK = msgBox.button(QMessageBox.Ok)
        buttonK.setIcon(QIcon())
        buttonK.setText('OK')
        ret = msgBox.exec_()
        return ret

# ASPECT CHANGED
    def aspect_group_clicked(self, button):
        if self.aspectGroup.id(button) == 0:
            self.aspect = ''
        elif self.aspectGroup.id(button) == 1:
            self.aspect = '_9x16'
        elif self.aspectGroup.id(button) == 2:
            self.aspect = '_4x3'

# MODE CHANGED
    def mode_group_clicked(self, button):
        self.mode = self.modeGroup.id(button)
        self.set_mode()

# ESTOP CHANGED
    def estop_group_clicked(self, button):
        self.estop = self.estopGroup.id(button)
        self.set_estop()

# ENSURE VALID MACHINE NAME
    def machine_name_changed(self,widget):
        chars = len(self.nameFile.text())
        if chars:
            char = self.nameFile.text()[chars - 1]
            if chars == 1 and not char.isalpha() and not char == '_':
                msg  = '\nThe machine name may only begin\n'
                msg += 'with a letter or an underscore\n\n'
                if self.dialog_ok('NAME ERROR', msg):
                    self.nameFile.setText(self.nameFile.text()[:chars - 1])
            elif not char.isalpha() and not char.isdigit() and not char == '_':
                msg  = '\nThe machine name must only consist of\n'
                msg += 'letters, numbers or underscores\n\n'
                if self.dialog_ok('NAME ERROR', msg):
                    self.nameFile.setText(self.nameFile.text()[:chars - 1])
            else:
                pass
                self.machineName = self.nameFile.text()
                self.configDir = '{}/{}'.format(self.configPath,self.machineName.lower())
                self.newIniFile = '{}/{}.ini'.format(self.configDir,self.machineName.lower())

# SELECT BASE INI FILE
    def ini_file_select_pressed(self):
        DIR = self.configPath
        if self.configureType == 'new':
            if os.path.dirname(self.halFile.text()):
                DIR = os.path.dirname(self.halFile.text())
        name, _ = QFileDialog.getOpenFileName(
                    parent=self,
                    caption=self.tr("Select a ini file"),
                    filter=self.tr('INI files (*.[iI][nN][iI]);;All Files (*)'),
                    directory=DIR
                    )
        if name:
            self.iniFile.setText(name)
            self.orgIniFile = name
        else:
            self.iniFile.setText('')
            self.orgIniFile = ''
            return
        if self.configureType == 'reconfigure':
            inFile = open(self.orgIniFile,'r')
            while 1:
                line = inFile.readline()
                if line.startswith('[EMC]'):
                    break
                elif not line:
                    print('[EMC] missing from {}'.format(self.orgIniFile))
                    return
            while 1:
                line = inFile.readline()
                if line.startswith('MACHINE'):
                    self.machineName = line.split('=')[1].strip()
                    self.configDir = '{}'.format(os.path.dirname(self.orgIniFile))
                    inFile.close()
                    break
                elif line.startswith('[') or not line:
                    inFile.close()
                    break
            self.modeSet = False
            self.populate_reconfigure()

# SELECT BASE HAL FILE
    def hal_file_select_pressed(self):
        if os.path.dirname(self.iniFile.text()):
            DIR = os.path.dirname(self.iniFile.text())
        else:
            DIR = self.configPath
        name, _ = QFileDialog.getOpenFileName(
                    parent=self,
                    caption=self.tr("Select a ini file"),
#                    filter=self.tr('HAL files (*.[hH][aA][lL] *.[tT][cC][lL]);;All Files (*)'),
                    filter=self.tr('HAL files (*.hal *.tcl);;All Files (*.*)'),
                    directory=DIR
                    )
        if name:
            self.halFile.setText(name)
            self.orgHalFile = name
            self.halExt = os.path.splitext(name)[1]
        else:
            self.halFile.setText('')
            self.orgHalFile = ''

# RETURN TO MAIN SCREEN
    def cancel_clicked(self,button):
        self.remove_widgets(self.layout)
        self.layout.addWidget(self.new, 0)
        self.layout.addWidget(self.rec, 1)
        self.layout.addWidget(self.can, 2)
        self.setWindowTitle('QtPlasmaC Setup')
        self.setFixedWidth(290)
        self.setFixedHeight(120)
#        print('HEIGHT', self.frameGeometry().height())
        screen = QDesktopWidget().screenGeometry()
        widget = self.geometry()
        x = screen.width()/2 - widget.width()/2
        y = screen.height()/2 - widget.height()/2
        self.move(x, y)

# BULK REMOVE WIDGETS
    def remove_widgets(self, layout):
        for i in reversed(range(layout.count())):
            item = layout.itemAt(i)
            if isinstance(item, QWidgetItem):
                item.widget().close()
            elif isinstance(item, QSpacerItem):
                pass
            else:
                self.remove_widgets(item.layout())
            layout.removeItem(item)

# SET CORRECT WIDGETS FOR PLASMAC MODE
    def set_mode(self):
        if self.mode == 0:
            self.modeLabel.setText('Use arc voltage for both Arc-OK and THC')
            for widget in [self.arcVoltLabel, self.arcVoltPin]:
                widget.show()
            for widget in [self.arcOkLabel, self.arcOkPin, self.moveUpLabel, self.moveUpPin, self.moveDownLabel, self.moveDownPin]:
                widget.hide()
        elif self.mode == 1:
            self.modeLabel.setText('Use arc ok for Arc-OK and arc voltage for THC')
            for widget in [self.arcVoltLabel, self.arcVoltPin, self.arcOkLabel, self.arcOkPin]:
                widget.show()
            for widget in [self.moveUpLabel, self.moveUpPin, self.moveDownLabel, self.moveDownPin]:
                widget.hide()
        elif self.mode == 2:
            self.modeLabel.setText('Use arc ok for Arc-OK and up/down signals for THC')
            for widget in [self.arcVoltLabel, self.arcVoltPin]:
                widget.hide()
            for widget in [self.arcOkLabel, self.arcOkPin, self.moveUpLabel, self.moveUpPin, self.moveDownLabel, self.moveDownPin]:
                widget.show()

# SET FOR ESTOP TYPE
    def set_estop(self):
        if self.estop == 0:
            self.estopLabel.setText('Estop is an indicator only')
        elif self.estop == 1:
            self.estopLabel.setText('Estop is hidden')
        elif self.estop == 2:
            self.estopLabel.setText('Estop is a button')

# CREATE A NEW CONFIG
    def on_create_clicked(self,button):
        if not self.check_entries():
            return
        if self.configureType == 'reconfigure':
            if not self.link_to_common_folder(): return
            self.reconfigure()
            self.cancel_clicked(None)
            self.dialog_ok('RECONFIGURE','\nReconfigure is complete.\n\n')
            return
        if not self.check_new_path(): return
        if not self.copy_machine_ini_file(): return
        if not self.copy_machine_hal_file(): return
        if not self.get_traj_info(): return
        if not self.get_kins_info(): return
        if not self.write_new_machine_ini_file(): return
        if not self.write_new_machine_hal_file(): return
        if not self.write_new_connections_hal_file(): return
        if not self.write_new_postgui_hal_file(): return
        if not self.write_new_material_file(): return
        if not self.write_new_prefs_file(): return
        if not self.link_to_common_folder(): return
        self.cancel_clicked(None)
        self.success_dialog()

# CHECK IF ENTRIES ARE VALID
    def check_entries(self):
        if self.configureType == 'new':
            if not self.nameFile.text():
                self.dialog_ok('ERROR','Machine name is required')
                return False
            if not self.halFile.text():
                self.dialog_ok('ERROR','HAL file is required')
                return False
        if not self.iniFile.text():
            self.dialog_ok('ERROR','INI file is required')
            return False
        if self.mode == 0 or self.mode == 1:
            if not self.arcVoltPin.text():
                self.dialog_ok('ERROR','Arc voltage is required for Mode {:d}'.format(self.mode))
                return False
        if self.mode == 1 or self.mode == 2:
            if not self.arcOkPin.text():
                self.dialog_ok('ERROR','Arc OK is required for Mode {:d}'.format(self.mode))
                return False
        if not self.torchPin.text():
            self.dialog_ok('ERROR','Torch on is required')
            return False
        if not self.ohmicInPin.text() and not self.floatPin.text():
            self.dialog_ok('ERROR','At least one of ohmic probe or float switch is required')
            return False
        if self.ohmicInPin.text() and not self.ohmicOutPin.text():
            self.dialog_ok('ERROR','Ohmic enable is required if ohmic probe is specified')
            return False
        if self.mode == 2:
            if not self.moveUpPin.text():
                self.dialog_ok('ERROR','Move up is required for Mode {:d}'.format(self.mode))
                return False
            if not self.moveDownPin.text():
                self.dialog_ok('ERROR','Move down is required for Mode {:d}'.format(self.mode))
                return False
        if self.laserX.text():
            try:
                self.laserXOffset = float(self.laserX.text())
            except:
                self.dialog_ok('ERROR','Laser X Offset is invalid')
                return False
        else:
            self.laserXOffset = 0.0
        if self.laserY.text():
            try:
                self.laserYOffset = float(self.laserY.text())
            except:
                self.dialog_ok('ERROR','Laser Y Offset is invalid')
                return False
        else:
            self.laserYOffset = 0.0
        if self.cameraX.text():
            try:
                self.cameraXOffset = float(self.cameraX.text())
            except:
                self.dialog_ok('ERROR','Camera X Offset is invalid')
                return False
        else:
            self.cameraXOffset = 0.0
        if self.cameraY.text():
            try:
                self.cameraYOffset = float(self.cameraY.text())
            except:
                self.dialog_ok('ERROR','Camera Y Offset is invalid')
                return False
        else:
            self.cameraYOffset = 0.0
        return True

# TEST IF REQUIRED PATH EXISTS
    def check_new_path(self):
        if self.configureType == 'new':
            if not os.path.exists('{}/backups'.format(self.configDir)):
                os.makedirs('{}/backups'.format(self.configDir))
            else:
                msg = '\nA configuration already exists in {}\n'.format(self.configDir)
                if not self.dialog_ok_cancel('CONFIGURATION EXISTS', msg, 'Overwrite', 'Cancel'):
                    return False
        return True

# COPY ORIGINAL INI FILE FOR INPUT AND BACKUP
    def copy_machine_ini_file(self):
        if os.path.dirname(self.orgIniFile) == '{}/backups'.format(self.configDir) and \
           os.path.basename(self.orgIniFile).startswith('_original_'):
            self.readIniFile = self.orgIniFile
        else:
            self.readIniFile = '{}/backups/_original_{}'.format(self.configDir,os.path.basename(self.orgIniFile))
            COPY(self.orgIniFile, self.readIniFile)

        if os.path.dirname(self.orgHalFile) == '{}/backups'.format(self.configDir) and \
           os.path.basename(self.orgHalFile).startswith('_original_'):
            self.readHalFile = self.orgHalFile
        else:
            self.readHalFile = '{}/backups/_original_{}'.format(self.configDir,os.path.basename(self.orgHalFile))
            COPY(self.orgHalFile, self.readHalFile)
        return True

# COPY ORIGINAL HAL FILE FOR INPUT AND BACKUP
    def copy_machine_hal_file(self):
        if os.path.dirname(self.orgIniFile) == '{}/backups'.format(self.configDir) and \
           os.path.basename(self.orgIniFile).startswith('_original_'):
            self.readIniFile = self.orgIniFile
        else:
            self.readIniFile = '{}/backups/_original_{}'.format(self.configDir,os.path.basename(self.orgIniFile))
            COPY(self.orgIniFile,self.readIniFile)

        if os.path.dirname(self.orgHalFile) == '{}/backups'.format(self.configDir) and \
           os.path.basename(self.orgHalFile).startswith('_original_'):
            self.readHalFile = self.orgHalFile
        else:
            self.readHalFile = '{}/backups/_original_{}'.format(self.configDir,os.path.basename(self.orgHalFile))
            COPY(self.orgHalFile,self.readHalFile)
        return True

# GET UNITS TYPE FROM [TRAJ] SECTION OF INI FILE COPY
    def get_traj_info(self):
        inFile = open(self.readIniFile,'r')
        while 1:
            line = inFile.readline()
            if '[TRAJ]' in line:
                break
            if not line:
                inFile.close()
                self.dialog_ok('ERROR','Cannot find [TRAJ] section in INI file')
                return False
        result = 0
        while 1:
            line = inFile.readline()
            if 'LINEAR_UNITS' in line:
                result += 1
                a,b = line.strip().replace(' ','').split('=')
                if b.lower() == 'inch':
                    self.iniFileUnits = 'inch'
                else:
                    self.iniFileUnits = 'mm'
            if line.startswith('[') or not line:
                if result == 1:
                    break
                else:
                    inFile.close()
                    self.dialog_ok('ERROR','Could not find LINEAR_UNITS in [TRAJ] section of INI file')
                    return False
        inFile.close()
        return True

# GET Z AXIS JOINT FROM [KINS] SECTION OF INI FILE COPY
    def get_kins_info(self):
        inFile = open(self.readIniFile, 'r')
        while 1:
            line = inFile.readline()
            if '[KINS]' in line:
                break
            if not line:
                inFile.close()
                self.dialog_ok('ERROR','Cannot find [KINS] section in INI file')
                self.W.destroy()
                return False
        kinsLine = jntsLine = ''
        while 1:
            line = inFile.readline()
            if line.startswith('KINEMATICS'):
                if 'kinstype' in line:
                    kinsLine = line.split('kinstype')[0]
                else:
                    kinsLine = line
            elif line.startswith('JOINTS'):
                jntsLine = line
            elif kinsLine and jntsLine:
                numJoints = int(jntsLine.strip().replace(' ','').split('=')[1])
                if 'coordinates' in kinsLine:
                    a, axes = kinsLine.lower().strip().replace(' ','').split('coordinates=')
                else:
                    axes = 'xyzabcuvw'[:numJoints]
                self.zJoint = axes.index('z')
                break
            elif line.startswith('[') or not line:
                inFile.close()
                if not kinsLine:
                    self.dialog_ok('ERROR','Could not find KINEMATICS in [KINS] section of INI file')
                if not jntsLine:
                    self.dialog_ok('ERROR','Could not find JOINTS in [KINS] section of INI file')
                self.W.destroy()
                return False
        inFile.close()
        return True

# CREATE NEW INI FILE
    def write_new_machine_ini_file(self):
        with open(self.readIniFile, 'r') as f:
            self.iniList = f.readlines()
        for line in self.iniList:
            if line.startswith('[QTPLASMAC]') or line.startswith('[PLASMAC]'):
                msg  = '\nThis configuration already supports PlasmaC\n'
                msg += '\nDid you mean to reconfigure?\n'
                self.dialog_ok('ERROR', msg)
                return False
        self.outFile = open(self.newIniFile,'w')
        self.ini_qtplasmac_section()
        self.ini_filter_section()
        self.ini_rs274ngc_section()
        self.ini_hal_section()
        self.ini_display_section()
        self.ini_emc_section()
        self.ini_emcio_section()
        self.ini_emcmot_section()
        self.ini_task_section()
        self.ini_kins_section()
        self.ini_traj_section()
        self.map_axes_to_joints()
        for a in 'XYZA':
            if a in self.axes:
                self.ini_axis_section(a)
        self.outFile.close()
        return True

    def ini_qtplasmac_section(self):
        self.outFile.write('[QTPLASMAC]\n' \
                           '# set the operating mode (default is 0)\n' \
                           'MODE                    = {}\n' \
                           '\n# set the estop type (0=indicator, 1=hidden, 2=button)\n' \
                           'ESTOP_TYPE              = {}\n' \
                           '\n# user buttons in the main window\n' \
                           'BUTTON_1_NAME           = OHMIC\TEST\n' \
                           'BUTTON_1_CODE           = ohmic-test\n' \
                           'BUTTON_2_NAME           = PROBE\TEST\n' \
                           'BUTTON_2_CODE           = probe-test 10\n' \
                           'BUTTON_3_NAME           = SINGLE\CUT\n' \
                           'BUTTON_3_CODE           = single-cut\n' \
                           'BUTTON_4_NAME           = NORMAL\CUT\n' \
                           'BUTTON_4_CODE           = cut-type\n' \
                           'BUTTON_5_NAME           = TORCH\PULSE\n' \
                           'BUTTON_5_CODE           = torch-pulse .5\n' \
                           'BUTTON_6_NAME           = \n' \
                           'BUTTON_6_CODE           = \n' \
                        '\n# powermax communications\n'.format(self.mode, self.estop) \
                        )
        if self.pmPortName.text():
            self.outFile.write('PM_PORT                 = {}\n'.format(self.pmPortName.text()))
        else:
            self.outFile.write('#PM_PORT                 = /dev/ttyUSB0\n')
        self.outFile.write('\n# laser touchoff\n')
        if self.laserXOffset or self.laserYOffset:
            self.outFile.write('LASER_TOUCHOFF          = X{:0.4f} Y{:0.4f}\n' \
                                   .format(self.laserXOffset, self.laserYOffset))
        else:
            self.outFile.write('#LASER_TOUCHOFF          = X0.0 Y0.0\n')
        self.outFile.write('\n# camera touchoff\n')
        if self.cameraXOffset or self.cameraYOffset:
            self.outFile.write('CAMERA_TOUCHOFF         = X{:0.4f} Y{:0.4f}\n' \
                                   .format(self.cameraXOffset, self.cameraYOffset))
        else:
            self.outFile.write('#CAMERA_TOUCHOFF         = X0.0 Y0.0\n')

    def ini_filter_section(self):
        self.outFile.write('\n# this section filters the gcode file to suit qtplasmac\n' \
                           '[FILTER]\n' \
                           'PROGRAM_EXTENSION       = .ngc,.nc,.tap GCode File (*.ngc, *.nc, *.tap)\n' \
                           'ngc                     = ./qtplasmac/qtplasmac_gcode.py\n' \
                           'nc                      = ./qtplasmac/qtplasmac_gcode.py\n' \
                           'tap                     = ./qtplasmac/qtplasmac_gcode.py\n' \
                           )

    def ini_rs274ngc_section(self):
        if self.iniFileUnits == 'inch':
            units = 'imperial'
        else:
            units = 'metric'
        self.outFile.write('\n[RS274NGC]\n' \
                           'RS274NGC_STARTUP_CODE   = o<{}_startup> call\n' \
                           'PARAMETER_FILE          = parameters.txt\n' \
                           'SUBROUTINE_PATH         = ./:./qtplasmac:../../nc_files/subroutines\n' \
                           'USER_M_PATH             = ./:./qtplasmac\n' \
                           .format(units) \
                           )

    def ini_hal_section(self):
        self.outFile.write('\n[HAL]\n' \
                           'TWOPASS                 = ON\n' \
                           'HALFILE                 = {0}{1}\n' \
                           'HALFILE                 = plasmac.tcl\n' \
                           '# plasma connections and users custom connections\n' \
                           'HALFILE                 = {0}_connections.hal\n' \
                           '# user customisation after GUI has loaded\n' \
                           'POSTGUI_HALFILE         = postgui.tcl\n' \
                           'HALUI                   = halui\n' \
                           .format(self.machineName.lower(),self.halExt)
                           )

    def ini_display_section(self):
        self.outFile.write('\n')
        start = False
        for line in self.iniList:
            if line.startswith('[DISPLAY]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    return
                if line.startswith('DISPLAY'):
                    self.outFile.write('DISPLAY                 = qtvcp qtplasmac{}\n'.format(self.aspect))
                elif line.startswith('MDI_HISTORY_FILE'):
                    self.outFile.write('MDI_HISTORY_FILE        = mdi_history.txt\n')
                elif line.startswith('OPEN_FILE'):
                    pass
                elif line.startswith('INTRO_'):
                    pass
                elif line.startswith('EDITOR'):
                    pass
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)

    def ini_emc_section(self):
        self.outFile.write('\n')
        start = False
        for line in self.iniList:
            if line.startswith('[EMC]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    return
                if line.startswith('MACHINE'):
                    self.outFile.write('MACHINE                 = {}\n'.format(self.machineName))
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)

    def ini_emcio_section(self):
        self.outFile.write('\n')
        start = False
        tool = False
        for line in self.iniList:
            if line.startswith('[EMCIO]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    break
                elif line.startswith('TOOL_TABLE'):
                    self.outFile.write('TOOL_TABLE              = tool.tbl\n')
                    tool = True
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)
        if not tool:
            self.outFile.write('TOOL_TABLE              = tool.tbl\n')

    def ini_emcmot_section(self):
        self.outFile.write('\n')
        start = False
        for line in self.iniList:
            if line.startswith('[EMCMOT]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    return
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)

    def ini_task_section(self):
        self.outFile.write('\n')
        start = False
        for line in self.iniList:
            if line.startswith('[TASK]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    return
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)

    def ini_kins_section(self):
        self.outFile.write('\n')
        start = False
        self.axes = 'XYZ'
        for line in self.iniList:
            if line.startswith('[KINS]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    return
                if line.startswith('KINEMATICS'):
                    line = line.strip()
                    if 'kinstype' in line:
                        line = line.split('kinstype')[0].strip()
                    if 'coordinates' in line:
                        a, b = line.split('coordinates')
                        self.axes = b.upper().replace(' ','').replace('=','')
                        line = '{} coordinates={}'.format(a.rstrip(' '), self.axes.lower())
                    self.outFile.write('{}\n'.format(line))
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)

    def ini_traj_section(self):
        self.outFile.write('\n')
        start = False
        spindles = False
        for line in self.iniList:
            if line.startswith('[TRAJ]'):
                start = True
                self.outFile.write(line)
            elif start:
                if not line or line.startswith('['):
                    if not spindles:
                        self.outFile.write('SPINDLES                = 3\n')
                    return
                if line.startswith('SPINDLES'):
                    self.outFile.write('SPINDLES                = 3\n')
                elif line.lstrip(' ').strip() == '':
                    pass
                else:
                    self.outFile.write(line)

    def map_axes_to_joints(self):
        self.axisX, self.axisY, self.axisZ, self.axisA = [], [], [], []
        for a in range(len(self.axes)):
            self['axis{}'.format(self.axes[a])].append(a)

    def ini_axis_section(self, axis):
        for joint in self['axis{}'.format(axis)]:
            self['j{}list'.format(joint)] = []
            start = False
            for line in self.iniList:
                if line.startswith('[JOINT_{}]'.format(joint)):
                    start = True
                    self['j{}list'.format(joint)].append(line)
                elif start:
                    if not line or line.startswith('['):
                        break
                    elif line.startswith('MIN_LIMIT'):
                        self['j{}minLim'.format(joint)] = str(line).split('=')[1].strip()
                    elif line.startswith('MAX_LIMIT'):
                        self['j{}maxLim'.format(joint)] = str(line).split('=')[1].strip()
                    elif line.startswith('MAX_VELOCITY'):
                        self['j{}maxVel'.format(joint)] = str(line).split('=')[1].strip()
                    elif line.startswith('MAX_ACCELERATION'):
                        self['j{}maxAcc'.format(joint)] = str(line).split('=')[1].strip()
                    self['j{}list'.format(joint)].append(line)
        self.outFile.write('\n[AXIS_{}]\n'.format(axis))
        self.outFile.write('MIN_LIMIT               = {}\n'.format(    self['j{}minLim'.format(self['axis{}'.format(axis)][0])]    ))
        self.outFile.write('MAX_LIMIT               = {}\n'.format(    self['j{}maxLim'.format(self['axis{}'.format(axis)][0])]    ))
        self.outFile.write('# set to double the value in the corresponding joint\n')
        self.outFile.write('MAX_VELOCITY            = {}\n'.format(    float(self['j{}maxVel'.format(self['axis{}'.format(axis)][0])]) * 2    ))
        self.outFile.write('# set to double the value in the corresponding joint\n')
        self.outFile.write('MAX_ACCELERATION        = {}\n'.format(    float(self['j{}maxAcc'.format(self['axis{}'.format(axis)][0])]) * 2    ))
        self.outFile.write('# shares the above two equally between the joint and the offset\n')
        self.outFile.write('OFFSET_AV_RATIO         = 0.5\n')
        for joint in self['axis{}'.format(axis)]:
            self.outFile.write('\n')
            for line in self['j{}list'.format(joint)]:
                if line.strip() == '':
                    pass
                elif 'slats'in line:
                    pass
                elif line.startswith('MIN_LIMIT'):
                    self.outFile.write('# this should be below the top of your slats\n')
                    self.outFile.write(line)
                elif line.startswith('HOME '):
                    self.outFile.write('# this should be 10mm (0.4") below the MAX_LIMIT\n')
                    self.outFile.write(line)
                else:
                    self.outFile.write(line)

# CREATE NEW HAL FILE
    def write_new_machine_hal_file(self):
        outFile = open('{}/{}{}'.format(self.configDir,self.machineName.lower(),self.halExt),'w')
        with open(self.readHalFile, 'r') as f:
            self.halList = f.readlines()
        for line in self.halList:
        # add spindles to motmod load parameters
            if 'servo_period_nsec' in line:
                if 'num_spindles' in line:
                    line = line.split('num_spindles')[0].strip()
                if self.readHalFile.split('.')[1].lower() == 'tcl':
                    line = '{} num_spindles=$::TRAJ(SPINDLES)\n'.format(line.strip())
                else:
                    line = '{} num_spindles=[TRAJ]SPINDLES\n'.format(line.strip())
                outFile.write(line)
        # remove spindle lines
            elif 'spindle' in line.lower():
                pass
        # remove halui lines
            elif 'halui.machine' in line.lower() or 'halui.program' in line.lower():
                pass
        # remove toolchange lines
            elif 'hal_manualtoolchange' in line or 'iocontrol.0.tool' in line:
                pass
            else:
                outFile.write(line)
        outFile.write('\n# qtplasmac toolchange passthrough\n'
                      'net tool:change iocontrol.0.tool-change  => iocontrol.0.tool-changed\n'
                      'net tool:prep   iocontrol.0.tool-prepare => iocontrol.0.tool-prepared\n')
        outFile.close()
        return True

# CREATE A CONNECTIONS.HAL FILE FOR PLASMAC CONNECTIONS TO THE MACHINE
    def write_new_connections_hal_file(self):
        with open('{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()), 'w') as outFile:
            outFile.write('# Keep your plasmac i/o connections here to prevent them from\n' \
                          '# being overwritten by updates or pncconf/stepconf changes.\n' \
                          '# Other customisations may be placed here as well\n' \
                          '\n#***** DEBOUNCE FOR THE INPUTS *****\n' \
                          'loadrt dbounce names=db_breakaway,db_float,db_ohmic,db_arc-ok\n' \
                          'addf db_float     servo-thread\n' \
                          'addf db_ohmic     servo-thread\n' \
                          'addf db_breakaway servo-thread\n' \
                          'addf db_arc-ok    servo-thread\n' \
                          '# for the float and ohmic inputs each increment in delay is\n' \
                          '# is a 0.001mm (0.00004") increase in any probed height result\n' \
                          'setp db_float.delay     5\n' \
                          'setp db_ohmic.delay     5\n' \
                          'setp db_breakaway.delay 5\n' \
                          'setp db_arc-ok.delay    5\n' \
                          '\n#***** ARC VOLTAGE LOWPASS FILTER *****\n' \
                          '# Only use this if comprehensive testing shows that it is required\n' \
                          'setp plasmac.lowpass-frequency 0\n' \
                          '\n#***** THE JOINT ASSOCIATED WITH THE Z AXIS *****\n')
            outFile.write('net plasmac:axis-position joint.{:d}.pos-fb => plasmac.axis-z-position\n\n'.format(self.zJoint))
            outFile.write('#***** PLASMA CONNECTIONS FOR MODE {} *****\n'.format(self.mode))
            if self.arcVoltPin.text() and (self.mode == 0 or self.mode == 1):
                outFile.write('net plasmac:arc-voltage-in {} => plasmac.arc-voltage-in\n'.format(self.arcVoltPin.text()))
            if self.arcOkPin.text() and (self.mode == 1 or self.mode == 2):
                outFile.write('net plasmac:arc-ok-in {} => db_arc-ok.in\n'.format(self.arcOkPin.text()))
            if self.floatPin.text():
                outFile.write('net plasmac:float-switch {} => db_float.in\n'.format(self.floatPin.text()))
            elif not self.floatPin.text():
                outFile.write('# net plasmac:float-switch {YOUR_FLOAT_SWITCH_PIN} => db_float.in\n')
            if self.breakPin.text():
                outFile.write('net plasmac:breakaway {} => db_breakaway.in\n'.format(self.breakPin.text()))
            elif not self.breakPin.text():
                outFile.write('# net plasmac:breakaway {YOUR_BREAKAWAY_PIN} => db_breakaway.in\n')
            if self.ohmicInPin.text():
                outFile.write('net plasmac:ohmic-probe {} => db_ohmic.in\n'.format(self.ohmicInPin.text()))
            elif not self.ohmicInPin.text():
                outFile.write('# net plasmac:ohmic-probe {YOUR_OHMIC_PROBE_PIN} => db_ohmic.in\n')
            if self.ohmicOutPin.text():
                outFile.write('net plasmac:ohmic-enable plasmac.ohmic-enable  => {}\n'.format(self.ohmicOutPin.text()))
            elif not self.ohmicOutPin.text():
                outFile.write('# net plasmac:ohmic-enable plasmac.ohmic-enable  => {YOUR_OHMIC_ENABLE_PIN}\n')
            if self.torchPin.text():
                outFile.write('net plasmac:torch-on => {}\n'.format(self.torchPin.text()))
            if self.moveUpPin.text() and self.mode == 2:
                outFile.write('net plasmac:move-up {} => plasmac.move-up\n'.format(self.moveUpPin.text()))
            if self.moveDownPin.text() and self.mode == 2:
                outFile.write('net plasmac:move-down {} => plasmac.move-down\n'.format(self.moveDownPin.text()))
            outFile.write('\n#***** SCRIBE CONNECTIONS *****\n')
            if self.scribeArmPin.text():
                outFile.write('net plasmac:scribe-arm plasmac.scribe-arm => {}\n'.format(self.scribeArmPin.text()))
            else:
                outFile.write('# net plasmac:scribe-arm plasmac.scribe-arm => {YOUR_SCRIBE_ARMING_OUTPUT}\n')
            if self.scribeOnPin.text():
                outFile.write('net plasmac:scribe-on  plasmac.scribe-on  => {}\n'.format(self.scribeOnPin.text()))
            else:
                outFile.write('# net plasmac:scribe-on  plasmac.scribe-on  => {YOUR_SCRIBE_ON_OUTPUT}\n')
            outFile.write('\n#***** PUT YOUR CUSTOM CONNECTION BELOW HERE *****\n')
        return True

# CREATE A POSTGUI.TCL HAL FILE IF NEW CONFIG OR FILE NOT EXISTING
    def write_new_postgui_hal_file(self):
        if self.configureType == 'new' or not os.path.exists('{}/postgui.tcl'.format(self.configDir)):
            with open('{}/postgui.tcl'.format(self.configDir), 'w') as outFile:
                outFile.write(\
                    '# Keep your post GUI customisations here to prevent them from\n' \
                    '# being overwritten by updates or pncconf/stepconf changes.\n' \
                    '\n# As an example:\n' \
                    '# You currently have a plasmac:thc-enable signal which connects the\n' \
                    '# plasmac_run.thc-enable-out output to the plasmac.thc-enable input.\n' \
                    '# You want to connect the thc-enable pin of the plasmac component\n' \
                    '# to a switch on your machine rather than it be controlled from the GUI.\n' \
                    '\n# First disconnect the GUI button from the plasmac:thc-enable signal:\n' \
                    '# unlinkp qtplasmac.thc_enable\n' \
                    '\n# Then connect the plasmac:thc-enable signal to your switch:\n' \
                    '# net plasmac:thc-enable your.switch-pin\n' \
                    )
        return True

# CREATE A NEW MATERIAL FILE IF NEW CONFIG OR FILE NOT EXISTING
    def write_new_material_file(self):
        materialFile = '{}/{}_material.cfg'.format(self.configDir,self.machineName.lower())
        if os.path.exists(materialFile) and not self.configureType == 'new':
            return True
        else:
            with open(materialFile, 'w') as outFile:
                outFile.write(self.material_header())
        return True

# CREATE A NEW PREFERENCES FILE IF NEW CONFIG OR FILE NOT EXISTING
    def write_new_prefs_file(self):
#        prefsFile = '{}/{}.prefs'.format(self.configDir,self.machineName.lower())
        prefsFile = '{}/qtplasmac.prefs'.format(self.configDir)
        if os.path.exists(prefsFile) and not self.configureType == 'new':
            return True
        else:
            with open(prefsFile, 'w') as outFile:
                outFile.write(\
                    '[NOTIFY_OPTIONS]\n' \
                    'notify_start_greeting = False\n' \
                    'notify_start_title = Welcome To QtPlasmaC\n' \
                    'notify_start_detail = This option can be changed in {}\n' \
                    'notify_start_timeout = 5\n' \
                    .format(prefsFile))
        return True

# RENAME ANY OLD FILES/FOLDERS AND MAKE LINK TO COMMON FOLDER
    def link_to_common_folder(self):
        for dir in ['qtplasmac', 'plasmac', 'common']:
            oldDir = '{}/{}'.format(self.configDir, dir)
            if os.path.islink(oldDir):
                os.unlink(oldDir)
            elif os.path.exists(oldDir):
                os.rename(oldDir, '{}_old_{}'.format(oldDir, time.time()))
        os.symlink(self.commonPath, '{}/qtplasmac'.format(self.configDir))
        return True

# RECONFIGURE IF CORRECT DATA
    def reconfigure(self):
        if self.mode != self.oldMode or \
           self.oldPmPortName != self.pmPortName.text() or \
           self.laserX != self.oldLaserX or self.laserY != self.oldLaserY or \
           self.cameraX != self.oldCameraX or self.cameraY != self.oldCameraY:
            backupFile = '{}/backups/reconfigure_{}.{}'.format(os.path.dirname(self.orgIniFile), os.path.basename(self.orgIniFile), time.time())
            COPY(self.orgIniFile, backupFile)
            outFile = open(self.orgIniFile, 'w')
            with open(backupFile, 'r') as inFile:
                while 1:
                    line = inFile.readline()
                    if not line:
                        break
                    elif line.startswith('MODE') and self.mode != self.oldMode:
                        self.oldMode = self.mode
                        outFile.write('MODE                    = {}\n'.format(self.mode))
                    elif line.startswith('ESTOP_TYPE') and self.estop != self.oldEstop:
                        self.oldEstop = self.estop
                        outFile.write('ESTOP_TYPE              = {}\n'.format(self.estop))
                    elif line.startswith('PM_PORT') and self.oldPmPortName != self.pmPortName.text():
                        self.oldPmPortName = self.pmPortName.text()
                        outFile.write('PM_PORT                 = {}\n'.format(self.oldPmPortName))
                    elif line.startswith('LASER_TOUCHOFF') and (self.oldLaserX != self.laserX.text() or self.oldLaserY != self.laserY.text()):
                        self.oldLaserX = self.laserX.text()
                        self.oldLaserY = self.laserY.text()
                        outFile.write('LASER_TOUCHOFF          = X{}  Y{}\n'.format(self.oldLaserX, self.oldLaserY))
                    elif line.startswith('CAMERA_TOUCHOFF') and (self.oldCameraX != self.cameraX.text() or self.oldCameraY != self.cameraY.text()):
                        self.oldCameraX = self.cameraX.text()
                        self.oldCameraY = self.cameraY.text()
                        outFile.write('CAMERA_TOUCHOFF         = X{}  Y{}\n'.format(self.oldCameraX, self.oldCameraY))
                    elif line.startswith('DISPLAY') and self.aspect != self.oldAspect:
                        self.oldAspect = self.aspect
                        outFile.write('DISPLAY                 = qtvcp qtplasmac{}\n'.format(self.aspect))
                    else:
                        outFile.write(line)
        arcVoltMissing = True
        arcOkMissing = True
        moveUpMissing = True
        moveDownMissing = True
        newConex = '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())
        oldConex = '{}/backups/reconfigure_{}_connections.hal.{}'.format(self.configDir,self.machineName.lower(), time.time())
        COPY(newConex, oldConex)
        outFile = open(newConex, 'w')
        with open(oldConex, 'r') as inFile:
            for line in inFile:
                newLine = line
                if 'PLASMA CONNECTIONS' in line:
                    outFile.write('#***** PLASMA CONNECTIONS FOR MODE {} *****\n'.format(self.mode))
                elif ':arc-voltage-in' in line:
                    arcVoltMissing = False
                    if self.arcVoltPin.text() and (self.mode == 0 or self.mode == 1):
                        if self.oldArcVoltPin != self.arcVoltPin.text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldArcVoltPin)
                            outFile.write('{}{}{}\n'.format(a, self.arcVoltPin.text(), b))
                            self.oldArcVoltPin = self.arcVoltPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':arc-ok-in' in line:
                    arcOkMissing = False
                    if self.arcOkPin.text() and (self.mode == 1 or self.mode == 2):
                        if self.oldArcOkPin != self.arcOkPin.text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldArcOkPin)
                            outFile.write('{}{}{}\n'.format(a, self.arcOkPin.text(), b))
                            self.oldArcOkPin = self.arcOkPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':ohmic-probe' in line:
                    if self.ohmicInPin.text():
                        if self.oldOhmicInPin != self.ohmicInPin.text():
                            a, b = line.strip('#').strip().split(self.oldOhmicInPin)
                            outFile.write('{}{}{}\n'.format(a, self.ohmicInPin.text(), b))
                            self.oldOhmicInPin = self.ohmicInPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':ohmic-enable' in line:
                    if self.ohmicOutPin.text():
                        if self.oldOhmicOutPin != self.ohmicOutPin.text():
                            a, b = line.strip('#').strip().split(self.oldOhmicOutPin)
                            outFile.write('{}{}{}\n'.format(a, self.ohmicOutPin.text(), b))
                            self.oldOhmicOutPin = self.ohmicOutPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':float-switch' in line:
                    if self.floatPin.text():
                        if self.oldFloatPin != self.floatPin.text():
                            a, b = line.strip('#').strip().split(self.oldFloatPin)
                            outFile.write('{}{}{}\n'.format(a, self.floatPin.text(), b))
                            self.oldFloatPin = self.floatPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':breakaway' in line:
                    if self.breakPin.text():
                        if self.oldBreakPin != self.breakPin.text():
                            a, b = line.strip('#').strip().split(self.oldBreakPin)
                            outFile.write('{}{}{}\n'.format(a, self.breakPin.text(), b))
                            self.oldBreakPin = self.breakPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':torch-on' in line:
                    if self.torchPin.text():
                        if self.oldTorchPin != self.torchPin.text():
                            a = line.strip('#').rsplit(self.oldTorchPin, 1)[0]
                            outFile.write('{}{}\n'.format(a, self.torchPin.text()))
                            self.oldTorchPin = self.torchPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':move-up' in line:
                    moveUpMissing = False
                    if self.moveUpPin.text() and self.mode == 2:
                        if self.oldMoveUpPin != self.moveUpPin.text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldMoveUpPin)
                            outFile.write('{}{}{}\n'.format(a, self.moveUpPin.text(), b))
                            self.oldMoveUpPin = self.moveUpPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':move-down' in line:
                    moveDownMissing = False
                    if self.moveDownPin.text() and self.mode == 2:
                        if self.oldMoveDownPin != self.moveDownPin.text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldMoveDownPin)
                            outFile.write('{}{}{}\n'.format(a, self.moveDownPin.text(), b))
                            self.oldMoveDownPin = self.moveDownPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':scribe-arm' in line:
                    if self.scribeArmPin.text():
                        if self.oldScribeArmPin != self.scribeArmPin.text():
                            a, b = line.strip('#').strip().split(self.oldScribeArmPin)
                            outFile.write('{}{}{}\n'.format(a, self.scribeArmPin.text(), b))
                            self.oldScribeArmPin = self.scribeArmPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif ':scribe-on' in line:
                    if self.scribeOnPin.text():
                        if self.oldscribeOnPin != self.scribeOnPin.text():
                            a, b = line.strip('#').strip().split(self.oldscribeOnPin)
                            outFile.write('{}{}{}\n'.format(a, self.scribeOnPin.text(), b))
                            self.oldscribeOnPin = self.scribeOnPin.text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'SCRIBE CONNECTIONS' in line:
                    blank = False
                    if (self.mode == 0 or self.mode == 1) and arcVoltMissing and self.arcVoltPin.text():
                        outFile.write('net plasmac:arc-voltage-in {} => plasmac.arc-voltage-in\n'.format(self.arcVoltPin.text()))
                        self.oldArcVoltPin = self.arcVoltPin.text()
                        blank = True
                    if (self.mode == 1 or self.mode) == 2 and arcOkMissing and self.arcOkPin.text():
                        outFile.write('net plasmac:arc-ok-in {} => plasmac.arc-ok-in\n'.format(self.arcOkPin.text()))
                        self.oldArcOkPin = self.arcOkPin.text()
                        blank = True
                    if self.mode == 2 and moveUpMissing and self.moveUpPin.text():
                        outFile.write('net plasmac:move-up {} => plasmac.move-up\n'.format(self.moveUpPin.text()))
                        self.oldMoveUpPin = self.moveUpPin.text()
                        blank = True
                    if self.mode == 2 and moveDownMissing and self.moveDownPin.text():
                        outFile.write('net plasmac:move-down {} => plasmac.move-down\n'.format(self.moveDownPin.text()))
                        self.oldMoveDownPin = self.moveDownPin.text()
                        blank = True
                    if blank:
                        outFile.write('\n')
                    outFile.write(line)

                else:
                    outFile.write(line)
        self.oldMode = self.mode
        return

# DISPLAY OLD CONFIG SETTINGS
    def populate_reconfigure(self):
        if not self.modeSet:
            with open(self.orgIniFile,'r') as inFile:
                while 1:
                    line = inFile.readline()
                    if line.startswith('[QTPLASMAC]'): break
                while 1:
                    line = inFile.readline()
                    if line.startswith('MODE'):
                        self.oldMode = int(line.split('=')[1].strip())
                        inFile.close()
                        break
                    elif line.startswith('[') or not line:
                        inFile.close()
                        break
            [self.mode0, self.mode1, self.mode2][self.oldMode].setChecked(True)
            self.mode = self.oldMode
            self.modeSet = True
        with open(self.orgIniFile,'r') as inFile:
            while 1:
                line = inFile.readline()
                if line.startswith('[DISPLAY]'): break
            while 1:
                line = inFile.readline()
                if line.startswith('DISPLAY'):
                    aspect = line.split('qtplasmac')[1].strip()
                    if aspect == '_4x3':
                        self.oldAspect = '_4x3'
                        self.aspect2.setChecked(True)
                    elif aspect == '_9x16':
                        self.oldAspect = '_9x16'
                        self.aspect1.setChecked(True)
                    else:
                        self.oldAspect = ''
                        self.aspect0.setChecked(True)
                    inFile.close()
                    break
                elif line.startswith('[') or not line:
                    inFile.close()
                    break
        with open(self.orgIniFile,'r') as inFile:
            while 1:
                line = inFile.readline()
                if line.startswith('[QTPLASMAC]'): break
            while 1:
                line = inFile.readline()
                if line.startswith('ESTOP_TYPE'):
                    estop = line.split('=')[1].strip()
                    if estop == '2':
                        self.oldEstop = '2'
                        self.estop2.setChecked(True)
                    elif estop == '1':
                        self.oldEstop = '1'
                        self.estop1.setChecked(True)
                    else:
                        self.oldEstop = '0'
                        self.estop0.setChecked(True)
                    inFile.close()
                    break
                elif line.startswith('[') or not line:
                    self.oldEstop = '0'
                    self.estop0.setChecked(True)
                    inFile.close()
                    break
        self.arcVoltPin.setText('')
        self.oldArcVoltPin = ''
        self.arcOkPin.setText('')
        self.oldArcOkPin = ''
        self.ohmicInPin.setText('')
        self.oldOhmicInPin = ''
        self.ohmicOutPin.setText('')
        self.oldOhmicOutPin = ''
        self.floatPin.setText('')
        self.oldFloatPin = ''
        self.breakPin.setText('')
        self.oldBreakPin = ''
        self.torchPin.setText('')
        self.oldTorchPin = ''
        self.moveUpPin.setText('')
        self.oldMoveUpPin = ''
        self.moveDownPin.setText('')
        self.oldMoveDownPin = ''
        self.scribeArmPin.setText('')
        self.oldScribeArmPin = ''
        self.scribeOnPin.setText('')
        self.oldScribeOnPin = ''
        self.pmPortName.setText('')
        self.oldPmPortName = ''
        self.laserX.setText('')
        self.oldLaserX = ''
        self.laserY.setText('')
        self.oldLaserY = ''
        self.cameraX.setText('')
        self.oldCameraX = ''
        self.cameraY.setText('')
        self.oldCameraY = ''
        # try:
        with open('{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()), 'r') as inFile:
            for line in inFile:
                if ':arc-voltage-in' in line:
                    self.oldArcVoltPin = (line.split('age-in', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.arcVoltPin.setText(self.oldArcVoltPin)
                elif ':arc-ok-in' in line:
                    self.oldArcOkPin = (line.split('plasmac:arc-ok-in', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.arcOkPin.setText(self.oldArcOkPin)
                elif ':ohmic-probe' in line:
                    self.oldOhmicInPin = (line.split('-probe', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.ohmicInPin.setText(self.oldOhmicInPin)
                elif ':ohmic-enable' in line:
                    self.oldOhmicOutPin = (line.strip().split(' ' )[-1].strip())
                    if not line.strip().startswith('#'):
                        self.ohmicOutPin.setText(self.oldOhmicOutPin)
                elif ':float-switch' in line:
                    self.oldFloatPin = (line.split('-switch', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.floatPin.setText(self.oldFloatPin)
                elif ':breakaway' in line:
                    self.oldBreakPin = (line.split('breakaway', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.breakPin.setText(self.oldBreakPin)
                elif ':torch-on' in line:
                    self.oldTorchPin = (line.strip().split(' ')[-1].strip())
                    if not line.strip().startswith('#'):
                        self.torchPin.setText(self.oldTorchPin)
                elif ':move-up' in line:
                    self.oldMoveUpPin = (line.split('move-up', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.moveUpPin.setText(self.oldMoveUpPin)
                elif ':move-down' in line:
                    self.oldMoveDownPin = (line.split('move-down', 1)[1].strip().split(' ', 1)[0].strip())
                    if not line.strip().startswith('#'):
                        self.moveDownPin.setText(self.oldMoveDownPin)
                elif ':scribe-arm' in line:
                    self.oldScribeArmPin = (line.strip().split(' ')[-1].strip())
                    if not line.strip().startswith('#'):
                        self.scribeArmPin.setText(self.oldScribeArmPin)
                elif ':scribe-on' in line:
                    self.oldscribeOnPin = (line.strip().split(' ')[-1].strip())
                    if not line.strip().startswith('#'):
                        self.scribeOnPin.setText(self.oldscribeOnPin)
        # except:
        #     self.iniFile.setText('')
        #     self.dialog_ok(
        #         'FILE ERROR',
        #         '\nCannot open connections file:\n'
        #         '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()))
        #     return
        with open(self.orgIniFile,'r') as inFile:
            while 1:
                line = inFile.readline()
                if line.startswith('[QTPLASMAC]'): break
            while 1:
                line = inFile.readline()
                if line.startswith('LASER_TOUCHOFF'):
                    line = line.lower().replace('x',' ').replace('y',' ').split('=')[1].strip()
                    axis = line.split(' ', 1)
                    self.oldLaserX = axis[0].strip()
                    self.laserX.setText(self.oldLaserX)
                    self.oldLaserY = axis[1].strip()
                    self.laserY.setText(self.oldLaserY)
                elif line.startswith('CAMERA_TOUCHOFF'):
                    line = line.lower().replace('x',' ').replace('y',' ').split('=')[1].strip()
                    axis = line.split(' ', 1)
                    self.oldCameraX = axis[0].strip()
                    self.cameraX.setText(self.oldCameraX)
                    self.oldCameraY = axis[1].strip()
                    self.cameraY.setText(self.oldCameraY)
                elif line.startswith('PM_PORT'):
                    self.oldPmPortName = line.split('=')[1].strip()
                    self.pmPortName.setText(self.oldPmPortName)
                elif line.startswith('[') or not line:
                    inFile.close()
                    break
        self.set_mode()

# SUCCESSFUL CONFIG COMPLETED
    def success_dialog(self):
        if 'usr' in self.appPath:
            cmd = 'linuxcnc'
        else:
            cmd = '{}/scripts/linuxcnc'.format(self.appPath.split('/bin')[0])
        self.dialog_ok(\
            'INSTALLATION',\
            '\nConfiguration is complete.\n\n' \
            'You can run this configuration from a terminal as follows:\n\n' \
            '{} {}/{}.ini \n\n' \
            'Before attempting any cutting you will need\n' \
            'to check all settings on the Parameters tab\n\n' \
            .format(cmd,self.configDir,self.machineName.lower()))

# CREATE ALL REQUIRED WIDGETS
    def create_widgets(self):
        self.setFixedWidth(836)
        self.setFixedHeight(674)
        screen = QDesktopWidget().screenGeometry()
        widget = self.geometry()
        x = screen.width()/2 - widget.width()/2
        y = screen.height()/2 - widget.height()/2
        self.move(x, y)
        self.vBL = QVBoxLayout()
        self.vBR = QVBoxLayout()
        headerBoxL = QVBoxLayout()
        headerL = QLabel('Mandatory Settings')
        headerL.setFixedWidth(400)
        headerL.setAlignment(Qt.AlignCenter | Qt.AlignVCenter)
        headerBoxL.addWidget(headerL)
        self.vBL.addLayout(headerBoxL)
        headerBoxR = QVBoxLayout()
        headerR = QLabel('Optional Settings')
        headerR.setFixedWidth(400)
        headerR.setAlignment(Qt.AlignCenter | Qt.AlignVCenter)
        headerBoxR.addWidget(headerR)
        self.vBR.addLayout(headerBoxR)
        nameLabel = QLabel('Machine Name:')
        nameLabel.setFixedHeight(24)
        nameLabel.setAlignment(Qt.AlignBottom)
        if self.configureType == 'new':
            self.nameVBox = QVBoxLayout()
            self.nameFile = QLineEdit()
            self.nameFile.setToolTip(\
                'The <b>name</b> of the new or existing machine.\n' \
                'If not existing, this creates a directory ~/linuxcnc/configs/<b>name</b>.\n' \
                '<b>name.ini</b> and <b>name.hal</b> are then written to this directory '\
                'as well as other required files and links to appplication files.\n\n')
            self.nameVBox.addWidget(nameLabel)
            self.nameVBox.addWidget(self.nameFile)
            self.vBL.addLayout(self.nameVBox)
        self.iniVBox = QVBoxLayout()
        if self.configureType == 'new':
            iniLabel = QLabel('INI file in existing working config:')
        else:
            iniLabel = QLabel('INI file of configuration to modify:')
        self.iniFileHBox = QHBoxLayout()
        self.iniFileButton = QPushButton('Select')
        self.iniFile = QLineEdit()
        self.iniVBox.addWidget(iniLabel)
        self.iniFileHBox.addWidget(self.iniFileButton)
        self.iniFileHBox.addWidget(self.iniFile)
        self.iniVBox.addLayout(self.iniFileHBox)
        self.vBL.addLayout(self.iniVBox)
        halLabel = QLabel('HAL file in existing working config:')
        if self.configureType == 'new':
            self.halVBox = QVBoxLayout()
            self.halFileHBox = QHBoxLayout()
            self.halFileButton = QPushButton('Select')
            self.halFile = QLineEdit()
            self.halVBox.addWidget(halLabel)
            self.halFileHBox.addWidget(self.halFileButton)
            self.halFileHBox.addWidget(self.halFile)
            self.halVBox.addLayout(self.halFileHBox)
            self.vBL.addLayout(self.halVBox)
        if self.configureType == 'new' or self.configureType == 'reconfigure':
            self.aspectVBox = QVBoxLayout()
            self.aspectLabel = QLabel('Monitor Aspect Ratio')
            self.aspectHBox = QHBoxLayout()
            self.aspectGroup = QButtonGroup()
            self.aspect0 = QRadioButton('16:9')
            self.aspect0.setFixedHeight(25)
            self.aspect0.setChecked(True)
            self.aspectHBox.addWidget(self.aspect0)
            self.aspectGroup.addButton(self.aspect0, 0)
            self.aspect1 = QRadioButton('9:16')
            self.aspectHBox.addWidget(self.aspect1)
            self.aspectGroup.addButton(self.aspect1, 1)
            self.aspect2 = QRadioButton('4:3')
            self.aspectHBox.addWidget(self.aspect2)
            self.aspectGroup.addButton(self.aspect2, 2)
            self.aspectVBox.addWidget(self.aspectLabel)
            self.aspectVBox.addLayout(self.aspectHBox)
            self.vBL.addLayout(self.aspectVBox)
            self.modeVBox = QVBoxLayout()
            self.modeLabel = QLabel('Use arc voltage for both Arc-OK and THC')
            self.modeHBox = QHBoxLayout()
            self.modeGroup = QButtonGroup()
            self.mode0 = QRadioButton('Mode: 0')
            self.mode0.setFixedHeight(25)
            self.mode0.setChecked(True)
            self.modeHBox.addWidget(self.mode0)
            self.modeGroup.addButton(self.mode0, 0)
            self.mode1 = QRadioButton('Mode: 1')
            self.modeHBox.addWidget(self.mode1)
            self.modeGroup.addButton(self.mode1, 1)
            self.mode2 = QRadioButton('Mode: 2')
            self.modeHBox.addWidget(self.mode2)
            self.modeGroup.addButton(self.mode2, 2)
            self.modeVBox.addWidget(self.modeLabel)
            self.modeVBox.addLayout(self.modeHBox)
            self.vBL.addLayout(self.modeVBox)
            self.estopVBox = QVBoxLayout()
            self.estopLabel = QLabel('Estop is an indicator only')
            self.estopHBox = QHBoxLayout()
            self.estopGroup = QButtonGroup()
            self.estop0 = QRadioButton('Estop: 0')
            self.estop0.setFixedHeight(25)
            self.estop0.setChecked(True)
            self.estopHBox.addWidget(self.estop0)
            self.estopGroup.addButton(self.estop0, 0)
            self.estop1 = QRadioButton('Estop: 1')
            self.estopHBox.addWidget(self.estop1)
            self.estopGroup.addButton(self.estop1, 1)
            self.estop2 = QRadioButton('Estop: 2')
            self.estopHBox.addWidget(self.estop2)
            self.estopGroup.addButton(self.estop2, 2)
            self.estopVBox.addWidget(self.estopLabel)
            self.estopVBox.addLayout(self.estopHBox)
            self.vBL.addLayout(self.estopVBox)
        if self.configureType == 'new' or self.configureType == 'reconfigure':
            self.arcVoltVBox = QVBoxLayout()
            self.arcVoltLabel = QLabel('Arc Voltage HAL pin: (float input)')
            self.arcVoltPin = QLineEdit()
            self.arcVoltVBox.addWidget(self.arcVoltLabel)
            self.arcVoltVBox.addWidget(self.arcVoltPin)
            self.vBL.addLayout(self.arcVoltVBox)
            self.arcOkVBox = QVBoxLayout()
            self.arcOkLabel = QLabel('Arc OK HAL pin: (bit input)')
            self.arcOkPin = QLineEdit()
            self.arcOkVBox.addWidget(self.arcOkLabel)
            self.arcOkVBox.addWidget(self.arcOkPin)
            self.vBL.addLayout(self.arcOkVBox)
            self.ohmicInVBox = QVBoxLayout()
            self.torchVBox = QVBoxLayout()
            torchLabel = QLabel('Torch On HAL pin: (bit output)')
            self.torchPin = QLineEdit()
            self.torchVBox.addWidget(torchLabel)
            self.torchVBox.addWidget(self.torchPin)
            self.vBL.addLayout(self.torchVBox)
            self.moveUpVBox = QVBoxLayout()
            self.moveUpLabel = QLabel('Move Up HAL pin: (bit input)')
            self.moveUpPin = QLineEdit()
            self.moveUpVBox.addWidget(self.moveUpLabel)
            self.moveUpVBox.addWidget(self.moveUpPin)
            self.vBL.addLayout(self.moveUpVBox)
            self.moveDownVBox = QVBoxLayout()
            self.moveDownLabel = QLabel('Move Down HAL pin: (bit input)')
            self.moveDownPin = QLineEdit()
            self.moveDownVBox.addWidget(self.moveDownLabel)
            self.moveDownVBox.addWidget(self.moveDownPin)
            self.vBL.addLayout(self.moveDownVBox)
            self.floatVBox = QVBoxLayout()
            floatLabel = QLabel('Float Switch HAL pin: (bit input)')
            self.floatPin = QLineEdit()
            self.floatVBox.addWidget(floatLabel)
            self.floatVBox.addWidget(self.floatPin)
            self.vBR.addLayout(self.floatVBox)
            self.breakVBox = QVBoxLayout()
            breakLabel = QLabel('Breakaway Switch HAL pin: (bit input)')
            self.breakPin = QLineEdit()
            self.breakVBox.addWidget(breakLabel)
            self.breakVBox.addWidget(self.breakPin)
            self.vBR.addLayout(self.breakVBox)
            ohmicInLabel = QLabel('Ohmic Probe HAL pin: (bit input)')
            self.ohmicInPin = QLineEdit()
            self.ohmicInVBox.addWidget(ohmicInLabel)
            self.ohmicInVBox.addWidget(self.ohmicInPin)
            self.vBR.addLayout(self.ohmicInVBox)
            self.ohmicOutVBox = QVBoxLayout()
            ohmicOutLabel = QLabel('Ohmic Probe Enable HAL pin: (bit output)')
            self.ohmicOutPin = QLineEdit()
            self.ohmicOutVBox.addWidget(ohmicOutLabel)
            self.ohmicOutVBox.addWidget(self.ohmicOutPin)
            self.vBR.addLayout(self.ohmicOutVBox)
            self.scribeArmVBox = QVBoxLayout()
            scribeArmLabel = QLabel('Scribe Arming HAL pin: (bit output)')
            self.scribeArmPin = QLineEdit()
            self.scribeArmVBox.addWidget(scribeArmLabel)
            self.scribeArmVBox.addWidget(self.scribeArmPin)
            self.vBR.addLayout(self.scribeArmVBox)
            self.scribeOnVBox = QVBoxLayout()
            scribeOnLabel = QLabel('Scribe On HAL pin: (bit output)')
            self.scribeOnPin = QLineEdit()
            self.scribeOnVBox.addWidget(scribeOnLabel)
            self.scribeOnVBox.addWidget(self.scribeOnPin)
            self.vBR.addLayout(self.scribeOnVBox)
            self.pmPortVBox = QVBoxLayout()
            pmPortLabel = QLabel('Powermax Com Port: (e.g. /dev/ttyUSB0)')
            self.pmPortName = QLineEdit()
            self.pmPortVBox.addWidget(pmPortLabel)
            self.pmPortVBox.addWidget(self.pmPortName)
            self.vBR.addLayout(self.pmPortVBox)
            self.laserVBox = QVBoxLayout()
            laserLabel = QLabel('Laser Alignment: (X and Y offsets)')
            self.laserHBox = QHBoxLayout()
            laserXLabel= QLabel('X:')
            self.laserX = QLineEdit()
            laserYLabel= QLabel('Y:')
            self.laserY = QLineEdit()
            self.laserHBox.addWidget(laserXLabel)
            self.laserHBox.addWidget(self.laserX)
            self.laserHBox.addWidget(laserYLabel)
            self.laserHBox.addWidget(self.laserY)
            self.laserVBox.addWidget(laserLabel)
            self.laserVBox.addLayout(self.laserHBox)
            self.vBR.addLayout(self.laserVBox)
            self.cameraVBox = QVBoxLayout()
            cameraLabel = QLabel('Camera Alignment: (X and Y offsets)')
            self.cameraHBox = QHBoxLayout()
            cameraXLabel= QLabel('X:')
            self.cameraX = QLineEdit()
            cameraYLabel= QLabel('Y:')
            self.cameraY = QLineEdit()
            self.cameraHBox.addWidget(cameraXLabel)
            self.cameraHBox.addWidget(self.cameraX)
            self.cameraHBox.addWidget(cameraYLabel)
            self.cameraHBox.addWidget(self.cameraY)
            self.cameraVBox.addWidget(cameraLabel)
            self.cameraVBox.addLayout(self.cameraHBox)
            self.vBR.addLayout(self.cameraVBox)
            for widget in [self.aspectLabel, iniLabel, halLabel, self.modeLabel, self.arcVoltLabel, \
                           self.arcOkLabel, torchLabel, self.moveUpLabel, self.moveDownLabel, \
                           self.estopLabel, floatLabel, breakLabel, ohmicInLabel, ohmicOutLabel, \
                           scribeArmLabel, scribeOnLabel, pmPortLabel, laserLabel, cameraLabel]:
                widget.setFixedHeight(24)
                widget.setAlignment(Qt.AlignBottom)
        blank = QLabel(' ')
        self.vBR.addWidget(blank)
        BB = QHBoxLayout()
        if self.configureType == 'new':
            self.create = QPushButton('Create')
        else:
            self.create = QPushButton('Reconfigure')
        self.cancel = QPushButton('Cancel')
        BB.addWidget(self.create)
        BB.addWidget(self.cancel)
        vSpaceL = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        self.vBL.addItem(vSpaceL)
        vSpaceR = QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Expanding)
        self.vBR.addItem(vSpaceR)
        self.vBR.addLayout(BB)
        self.layout.addLayout(self.vBL)
        self.layout.addLayout(self.vBR)
        if self.configureType == 'new':
            self.setWindowTitle('QtPlasmaC New Configuration')
            self.modeLabel.setText('Use arc voltage for both Arc-OK and THC')
            for widget in [self.arcVoltLabel, self.arcVoltPin]:
                widget.show()
            for widget in [self.arcOkLabel, self.arcOkPin, self.moveUpLabel, self.moveUpPin, self.moveDownLabel, self.moveDownPin]:
                widget.hide()
        else:
            self.setWindowTitle('QtPlasmaC Reconfigure')

# HEADER FOR MATERIAL FILE
    def material_header(self):
        return  '# plasmac material file\n' \
                '# example only, may be deleted\n' \
                '# items marked * are mandatory\n' \
                '# other items are optional and will default to 0\n' \
                '#[MATERIAL_NUMBER_1]  \n' \
                '#NAME               = \n' \
                '#KERF_WIDTH         = \n' \
                '#PIERCE_HEIGHT      = *\n' \
                '#PIERCE_DELAY       = *\n' \
                '#PUDDLE_JUMP_HEIGHT = \n' \
                '#PUDDLE_JUMP_DELAY  = \n' \
                '#CUT_HEIGHT         = *\n' \
                '#CUT_SPEED          = *\n' \
                '#CUT_AMPS           = \n' \
                '#CUT_VOLTS          = \n' \
                '#PAUSE_AT_END       = \n' \
                '#GAS_PRESSURE       = \n' \
                '#CUT_MODE           = \n' \
                '\n'

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = Configurator()
    w.show()
    sys.exit(app.exec_())
