#!/usr/bin/env python

'''
configurator.py

Copyright (C) 2019  Phillip A Carter

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

import pygtk
pygtk.require('2.0')
import gtk
import os
import sys
import shutil

class configurator:

    def __init__(self):
        self.W = gtk.Window()
        self.W.set_title('PlasmaC Configurator')
        self.W.connect('delete_event', self.on_window_delete_event)
        self.W.set_position(gtk.WIN_POS_CENTER)
        if 'configs/by_machine/plasmac' not in os.path.realpath(sys.argv[0]):
            if self.dialog_ok(
                    'PATH ERROR',
                    '\nThe plasmac configurator must be run from the original source directory\n'
                    '\ne.g. ~/linuxcnc-plasmac/configs/by_machine/plasmac/configurator.py\n\n'):
                quit()
        configureType = self.start_dialog()
        if configureType == 1:
            self.upgrade = True
            if not self.dialog_ok_cancel(\
                    'Prerequisites',
                    '\nThis configurator will upgrade an existing plasmac configuration by:\n'
                    'Creating links to updated application files\n'
                    'Deleting links to redundant files\n'
                    'Making changes to the ini file if required\n\n'
                    'Existing machine HAL and custom HAL files will not be changed\n\n',
                    'Continue','Exit'):
                quit()
        elif configureType == 2:
            self.upgrade = False
            if not self.dialog_ok_cancel(
                    'Prerequisites',
                    '\nBefore using this configurator you should already have a\n'
                    'working configuration for your base machine.\n\n'
                    'The base machine should be fully operational.\n\n'
                    'If you don\'t have a working configuration then you need\n'
                    'to exit the configurator and create one.\n\n',
                    'Continue','Exit'):
                quit()
        else:
            quit()
        self.create_widgets()
        self.iniFile.connect('button_press_event', self.on_inifile_press_event)
        self.create.connect('clicked', self.on_create_clicked)
        self.cancel.connect('clicked', self.on_cancel_clicked)
        self.configPath = os.path.expanduser('~') + '/linuxcnc/configs'
        self.copyPath =  os.path.realpath(os.path.dirname(sys.argv[0]))
        self.gitPath = self.copyPath.split('/config')[0]
        self.orgIniFile = ''
        self.newIniPath = ''
        if not self.upgrade:
            self.nameFile.connect_after('focus-out-event', self.on_namefile_focus_out_event)
            self.nameFile.connect('changed', self.on_namefile_changed)
            self.halFile.connect('button_press_event', self.on_halfile_press_event)
            self.mode0.connect('toggled', self.on_mode0_toggled)
            self.mode1.connect('toggled', self.on_mode1_toggled)
            self.mode2.connect('toggled', self.on_mode2_toggled)
            self.mode = 0
            self.tabPanel0.connect('toggled', self.on_tabPanel0_toggled)
            self.tabPanel1.connect('toggled', self.on_tabPanel1_toggled)
            self.panel = 0
            self.newIniFile = ''
            self.orgHalFile = ''
            self.plasmacIniFile = self.copyPath + '/metric_plasmac.ini'
            self.inPlace = False

    def start_dialog(self):
        dialog = gtk.Dialog('SELECT',
                            self.W,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            ('New',2,
                             'Upgrade',1,
                             'Exit',0))
        label = gtk.Label('Is this a new configuration or an upgrade?')
        dialog.vbox.add(label)
        label.show()
        response = dialog.run()
        dialog.destroy()
        return response

    def dialog_ok_cancel(self,title,text,name1,name2):
        dialog = gtk.Dialog(title,
                            self.W,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            (name1, 1,
                            name2, 0)
                           )
        label = gtk.Label(text)
        dialog.vbox.add(label)
        label.show()
        response = dialog.run()
        dialog.destroy()
        return response

    def dialog_ok(self,title,error):
        dialog = gtk.Dialog(title,
                    self.W,
                    gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                    (gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
        label = gtk.Label(error)
        dialog.vbox.add(label)
        label.show()
        response = dialog.run()
        dialog.destroy()
        return response

    def on_window_delete_event(self,window,event):
        gtk.main_quit()

    def on_mode0_toggled(self,button):
        if button.get_active():
            self.mode = 0
            self.modeLabel.set_text('Use arc voltage for both arc-OK and THC')
            self.arcVoltVBox.show()
            self.arcOkVBox.hide()
            self.moveUpVBox.hide()
            self.moveDownVBox.hide()

    def on_mode1_toggled(self,button):
        if button.get_active():
            self.mode = 1
            self.modeLabel.set_text('Use arc ok for arc-OK and arc voltage for both arc-OK and THC')
            self.arcVoltVBox.show()
            self.arcOkVBox.show()
            self.moveUpVBox.hide()
            self.moveDownVBox.hide()

    def on_mode2_toggled(self,button):
        if button.get_active():
            self.mode = 2
            self.modeLabel.set_text('Use arc ok for arc-OK and up/down signals for THC')
            self.arcVoltVBox.hide()
            self.arcOkVBox.show()
            self.moveUpVBox.show()
            self.moveDownVBox.show()

    def on_tabPanel0_toggled(self,button):
        if button.get_active():
            self.panel = 0
            self.tabPanelLabel.set_text('Run Frame is a tab behind preview')

    def on_tabPanel1_toggled(self,button):
        if button.get_active():
            self.panel = 1
            self.tabPanelLabel.set_text('Run Frame is a panel at the side of the GUI')

    def on_namefile_changed(self,widget):
        chars = len(self.nameFile.get_text())
        if chars:
            char = self.nameFile.get_text()[chars - 1]
            if chars == 1 and not char.isalpha() and not char == '_':
                if self.dialog_ok(
                        'NAME ERROR',
                        '\nThe machine name may only begin\n'
                        'with a letter or an underscore\n\n'):
                    self.nameFile.set_text(self.nameFile.get_text()[:chars - 1])
            elif not char.isalpha() and not char.isdigit() and not char == '_':
                if self.dialog_ok(
                        'NAME ERROR',
                        '\nThe machine name must only consist of\n'
                        'letters, numbers or underscores\n\n'):
                    self.nameFile.set_text(self.nameFile.get_text()[:chars - 1])
            else:
                pass

    def on_namefile_focus_out_event(self,widget,event):
        self.machineName = self.nameFile.get_text()
        self.newIniPath = '{}/{}'.format(self.configPath,self.machineName.lower())
        self.newIniFile = '{}/{}.ini'.format(self.newIniPath,self.machineName.lower())

    def on_inifile_press_event(self,button,event):
        self.dlg = gtk.FileChooserDialog('Open..', None, gtk.FILE_CHOOSER_ACTION_OPEN,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        if not self.upgrade and os.path.dirname(self.halFile.get_text()):
            self.dlg.set_current_folder(os.path.dirname(self.halFile.get_text()))
        else:
            self.dlg.set_current_folder(self.configPath)
        response = self.dlg.run()
        if response == gtk.RESPONSE_OK:
            self.iniFile.set_text(self.dlg.get_filename())
            self.orgIniFile = self.dlg.get_filename()
        else:
            self.iniFile.set_text('')
            self.orgIniFile = ''
        if self.upgrade:
            inFile = open(self.orgIniFile,'r')
            while 1:
                line = inFile.readline()
                if line.startswith('[EMC]'): break
            while 1:
                line = inFile.readline()
                if line.startswith('MACHINE'):
                    self.machineName = line.split('=')[1].strip()
                    inFile.close()
                    break
                elif line.startswith('[') or not line:
                    inFile.close()
                    break
        self.dlg.destroy()

    def on_halfile_press_event(self,button,event):
        self.dlg = gtk.FileChooserDialog('Save..', None, gtk.FILE_CHOOSER_ACTION_OPEN,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        if os.path.dirname(self.iniFile.get_text()):
            self.dlg.set_current_folder(os.path.dirname(self.iniFile.get_text()))
        else:
            self.dlg.set_current_folder(self.configPath)
        response = self.dlg.run()
        if response == gtk.RESPONSE_OK:
            self.halFile.set_text(self.dlg.get_filename())
            self.orgHalFile = self.dlg.get_filename()
            self.halExt = os.path.splitext(self.orgHalFile)[1]
        else:
            self.halFile.set_text('')
            self.orgHalFile = ''
        self.dlg.destroy()

    def on_cancel_clicked(self,button):
        gtk.main_quit()

    def b4_pmt_tpt(self):
        inFile = open(self.orgIniFile,'r')
        while 1:
            line = inFile.readline()
            if line.startswith('[PLASMAC]'): break
        while 1:
            line = inFile.readline()
            if line.startswith('PAUSED_MOTION_SPEED'):
                inFile.close()
                return True
            elif line.startswith('[') or not line:
                inFile.close()
                return False

    # check existing version so we know what to upgrade
    def check_version(self):
        # see if this is a version before creating {MACHINE}_connections.hal
        if not os.path.exists('{}/{}_connections.hal'.format(os.path.dirname(self.orgIniFile),self.machineName.lower())):
            return 0.0
        # if version before changing paused_motion_speed and torch_pulse_time
        elif not self.b4_pmt_tpt():
            return 0.1
        # must be the latest version
        else:
            return 0.2

    def on_create_clicked(self,button):
        if not self.check_entries(): return
        display = self.get_display()
        if display == None: return
        if not self.check_new_path(): return
        if self.upgrade:
            version = self.check_version()
            if version == 0.0:
                self.upgrade_connections_hal_00()
            self.upgrade_ini_file(version,display)
            self.upgrade_material_file()
            if not self.make_links(display): return
            self.dialog_ok('SUCCESS','\nUpgrade is complete.\n\n')
            return
        if not self.copy_ini_and_hal_files(): return
        if not self.get_traj_info(self.readIniFile,display): return
        if not self.get_joint_info(self.readIniFile): return
        if not self.write_new_hal_file(): return
        if not self.write_connections_hal_file(): return
        if not self.write_postgui_hal_file(): return
        if not self.write_newini_file(display): return
        if not self.make_links(display): return
        if not self.write_material_file(): return
        self.print_success()

    def check_entries(self):
        # check if entries are valid
        if self.upgrade:
            if not self.iniFile.get_text():
                self.dialog_ok('ERROR','INI file is required')
                return False
        else:
            if not self.nameFile.get_text():
                self.dialog_ok('ERROR','Machine name is required')
                return False
            if not self.iniFile.get_text():
                self.dialog_ok('ERROR','INI file is required')
                return False
            if not self.halFile.get_text():
                self.dialog_ok('ERROR','HAL file is required')
                return False
            if self.mode == 0 or self.mode == 1:
                if not self.arcVoltPin.get_text():
                    self.dialog_ok('ERROR','Arc voltage is required for Mode {:d}'.format(self.mode))
                    return False
            if self.mode == 1 or self.mode == 2:
                if not self.arcOkPin.get_text():
                    self.dialog_ok('ERROR','Arc OK is required for Mode {:d}'.format(self.mode))
                    return False
            if not self.ohmicInPin.get_text() and not self.floatPin.get_text():
                self.dialog_ok('ERROR','At least one of ohmic probe or float switch is required')
                return False
            if self.ohmicInPin.get_text() and not self.ohmicOutPin.get_text():
                self.dialog_ok('ERROR','Ohmic enable is required if ohmic probe is specified')
                return False
            if not self.torchPin.get_text():
                self.dialog_ok('ERROR','Torch on is required')
                return False
            if self.mode == 2:
                if not self.moveUpPin.get_text():
                    self.dialog_ok('ERROR','Move up is required for Mode {:d}'.format(self.mode))
                    return False
                if not self.moveDownPin.get_text():
                    self.dialog_ok('ERROR','Move down is required for Mode {:d}'.format(self.mode))
                    return False
        return True

    def get_display(self):
        # get the display GUI
        inFile = open('{}'.format(self.orgIniFile), 'r')
        while 1:
            line = inFile.readline()
            if line.startswith('[DISPLAY]'):
                break
            if not line:
                inFile.close()
                self.dialog_ok('ERROR','Cannot find [DISPLAY] section in INI file')
                return None
        while 1:
            line = inFile.readline()
            if line.startswith('DISPLAY'):
                if 'axis' in line.lower():
                    inFile.close()
                    return 'axis'
                elif 'gmoccapy' in line.lower():
                    inFile.close()
                    return 'gmoccapy'
                else:
                    inFile.close()
                    self.dialog_ok('ERROR','Cannot find a valid display in INI file')
                    return None
            elif line.startswith('[') or not line:
                inFile.close()
                self.dialog_ok('ERROR','Cannot find \"DISPLAY =\" in INI file')
                return None

    def check_new_path(self):
        # test if path exists
        if not self.upgrade:
            if not os.path.exists(self.newIniPath):
                os.makedirs(self.newIniPath)
            else:
                if not self.dialog_ok_cancel('CONFIGURATION EXISTS',\
                                             '\nA configuration already exists in {}\n'\
                                             .format(self.newIniPath),'Overwrite','Back'):
                    return False
        return True

    def upgrade_connections_hal_00(self):
        # create a {MACHINE}_connections.hal for an upgrade from 00
        inFile = open('{}/plasmac.hal'.format(os.path.dirname(self.orgIniFile)), 'r')
        outFile = open('{}/{}_connections.hal'.format(os.path.dirname(self.orgIniFile),self.machineName.lower()), 'w')
        outFile.write(\
            '# Keep your plasmac i/o connections here to prevent them from\n'\
            '# being overwritten by updates or pncconf/stepconf changes\n\n'\
            '# Other customisations may be placed here as well\n'\
            '# This file is built by the configurator in your configuration directory\n\n')
        for line in inFile:
            if ' '.join(line.split()).startswith('loadrt debounce'):
                outFile.write(\
                    '#***** DEBOUNCE FOR THE FLOAT SWITCH *****\n'\
                    '# the lower the delay here the better\n'\
                    '# each 1 is a 0.001mm (0.00004") increase in probed height result\n'\
                    + line)
            elif ' '.join(line.split()).startswith('setp debounce.0.delay'):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('addf debounce.0'):
                outFile.write(line + '\n')
            elif ' '.join(line.split()).startswith('net plasmac:axis-position '):
                outFile.write(\
                    '#***** arc voltage lowpass cutoff frequency *****\n'\
                    '#***** change to the cutoff frequency you require *****\n'\
                    '#setp plasmac.lowpass-frequency 0\n\n'\
                    '#***** the joint associated with the Z axis *****\n'\
                     + line + '\n')
            elif ' '.join(line.split()).startswith('net plasmac:arc-voltage-in '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:arc-ok-in '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:float-switch '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:breakaway '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:ohmic-probe '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:ohmic-enable '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:torch-on ') and not 'plasmac.torch-on' in line:
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:move-down '):
                outFile.write(line)
            elif ' '.join(line.split()).startswith('net plasmac:move-up '):
                outFile.write(line)
        inFile.close()
        outFile.close()

    def upgrade_ini_file(self,version,display):
        # create a new ini file for an upgrade
        if version < 0.1:
            shutil.copy(self.orgIniFile,'{}.old0'.format(self.orgIniFile))
            inFile = open('{}.old0'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if ' '.join(line.strip().split()) == 'HALFILE = plasmac.hal':
                    outFile.write(\
                        'HALFILE = plasmac.hal\n'\
                        '# the plasmac machine connections\n'\
                        'HALFILE = {}_connections.hal\n'\
                        .format(self.machineName.lower()))
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        if version == 0.1:
            shutil.copy(self.orgIniFile,'{}.old01'.format(self.orgIniFile))
            inFile = open('{}.old01'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if line.startswith('# multiply cut-fe'): pass
                elif line.startswith('PAUSED-MOTION'): pass
                elif line.startswith('# torch on time'): pass
                elif line.startswith('TORCH-PULSE-TIM'): pass
                elif line.startswith('# for the five ')  or line.startswith('# for the four '):
                    outFile.write(\
                        '# percentage of cut-feed-rate used for paused motion speed\n'\
                        'PAUSED_MOTION_SPEED     = 50\n'\
                        '\n'\
                        '# torch on time when manual pulse requested\n'\
                        'TORCH_PULSE_TIME        = 1.0\n\n')
                elif line.startswith('EMBED_TAB_NAME') and 'Plasma Run' in line:
                    outFile.write(\
                        '\n'\
                        '# use one of the next two\n'\
                        '# run frame in tab behind preview\n'\
                        'EMBED_TAB_NAME          = Plasma Run\n')
                elif line.startswith('EMBED_TAB_COMMAND') and 'plasmac_run.glade' in line:
                    outFile.write('EMBED_TAB_COMMAND       = gladevcp -c plasmac_run -x {XID} -u ./plasmac_run.py -H plasmac_run.hal plasmac_run_tab.glade\n')
                    if display == 'axis':
                        outFile.write(\
                            '# run frame on right\n'\
                            '#GLADEVCP                = -c plasmac_run -u ./plasmac_run.py -H plasmac_run.hal plasmac_run_panel.glade\n\n')
                    elif display == 'gmoccapy':
                        outFile.write(\
                            '# run frame on left\n'\
                            '#EMBED_TAB_LOCATION      = box_left\n'\
                            '#EMBED_TAB_COMMAND       = gladevcp -c plasmac_run -x {XID} -u ./plasmac_run.py -H plasmac_run.hal plasmac_run_panel.glade\n\n')
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()

    def upgrade_material_file(self):
        materialFile = '{}/{}_material.cfg'.format(os.path.dirname(self.orgIniFile),self.machineName.lower())
        if os.path.exists(materialFile):
            inFile = open(materialFile, 'r')
            mVersion = 0
            for line in inFile:
                if '[VERSION' in line:
                    mVersion = float(line.strip().strip(']').split(' ')[1])
            inFile.close()
            if mVersion == 1:
                shutil.copy(materialFile,'{}.old1'.format(materialFile))
                inFile = open('{}.old1'.format(materialFile), 'r')
                outFile = open(materialFile, 'w')
                outFile.write(self.material_header())
                while 1:
                    line = inFile.readline()
                    if line.startswith('[MATERIAL_NUMBER'):
                        outFile.write(line)
                        break
                    if not line:
                        inFile.close()
                        self.dialog_ok('ERROR','Cannot find a MATERIAL section in material file')
                        return False
                while 1:
                    line = inFile.readline()
                    if not line:
                        inFile.close()
                        return True
                    outFile.write(line)
                inFile.close()
                outFile.close()
        else:
            print('No material file to upgrade')

    def copy_ini_and_hal_files(self):
        # copy original INI and HAL files for input and backup
        if os.path.dirname(self.orgIniFile) == self.newIniPath and \
           os.path.basename(self.orgIniFile).startswith('_original_'):
            self.readIniFile = self.orgIniFile
        else:
            self.readIniFile = '{}/_original_{}'.format(self.newIniPath,os.path.basename(self.orgIniFile))
            shutil.copy(self.orgIniFile,self.readIniFile)

        if os.path.dirname(self.orgHalFile) == self.newIniPath and \
           os.path.basename(self.orgHalFile).startswith('_original_'):
            self.readHalFile = self.orgHalFile
        else:
            self.readHalFile = '{}/_original_{}'.format(self.newIniPath,os.path.basename(self.orgHalFile))
            shutil.copy(self.orgHalFile,self.readHalFile)
        return True

    def get_traj_info(self,readFile,display):
        # get some info from [TRAJ] section of INI file copy
        inFile = open(readFile,'r')
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
                if b.lower() == 'mm':
                    self.plasmacIniFile = '{}/{}/metric_plasmac.ini'.format(self.copyPath,display)
                else:
                    self.plasmacIniFile = '{}/{}/imperial_plasmac.ini'.format(self.copyPath,display)
            elif 'COORDINATES' in line:
                result += 10
                a,b = line.strip().replace(' ','').split('=')
                self.zJoint = b.lower().index('z')
                self.numJoints = len(b.strip())
            if line.startswith('[') or not line:
                if result == 11:
                    break
                else:
                    inFile.close()
                    if result == 1:
                        self.dialog_ok('ERROR','Could not find COORDINATES in [TRAJ] section of INI file')
                    else:
                        self.dialog_ok('ERROR','Could not find LINEAR_UNITS in [TRAJ] section of INI file')
                    return False
        inFile.close()
        return True

    def get_joint_info(self,readFile):
        # get some info from [JOINT_n] section of INI file copy
        inFile = open(readFile,'r')
        self.zVel = self.zAcc = 0
        while 1:
            line = inFile.readline()
            if '[JOINT_{:d}]'.format(self.zJoint) in line:
                break
            if not line:
                inFile.close()
                self.dialog_ok('ERROR','Cannot find [JOINT_{d}] section in INI file'.format(self.zJoint))
                return False
        result = 0
        while 1:
            line = inFile.readline()
            if line.startswith('MAX_VELOCITY'):
                result += 1
                a,b = line.strip().replace(' ','').split('=')
                self.zVel = b
            elif line.startswith('MAX_ACCELERATION'):
                result += 10
                a,b = line.strip().replace(' ','').split('=')
                self.zAcc = b
            if line.startswith('[') or not line:
                if result == 11:
                    break
                else:
                    inFile.close()
                    if result == 1:
                        self.dialog_ok('ERROR','Could not find MAX_ACCELERATION in [JOINT_{:d}] section of INI file'.format(self.zJoint))
                    else:
                        self.dialog_ok('ERROR','Could not find MAX_VELOCITY in [JOINT_{:d}] section of INI file'.format(self.zJoint))
                    return False
        inFile.close()
        return True

    def write_new_hal_file(self):
        #write new hal file with spindle.0.on commented out
        newHalFile = open('{}/{}{}'.format(self.newIniPath,self.machineName.lower(),self.halExt),'w')
        inHal = open(self.readHalFile,'r')
        for line in inHal:
            if 'spindle.0.on' in line:
                newHalFile.write('# ' + line)
            else:
                newHalFile.write(line)
        newHalFile.close()
        inHal.close()
        return True

    def write_connections_hal_file(self):
        # write a connections.hal file for plasmac connections to the machine
        with open('{}/{}_connections.hal'.format(self.newIniPath,self.machineName.lower()), 'w') as outFile:
            outFile.write(\
                '# Keep your plasmac i/o connections here to prevent them from\n'\
                '# being overwritten by updates or pncconf/stepconf changes\n\n'\
                '# Other customisations may be placed here as well\n'\
                '# This file is built by the configurator in your configuration directory\n\n'\
                '#***** debounce for the float switch *****\n'\
                '# the lower the delay here the better\n'\
                '# each 1 is a 0.001mm (0.00004") increase in probed height result\n'\
                'loadrt  debounce                cfg=3\n'\
                'setp    debounce.0.delay        5\n'\
                'addf    debounce.0              servo-thread\n\n'\
                '#***** arc voltage lowpass cutoff frequency *****\n'\
                '#***** change to the cutoff frequency you require *****\n'\
                '#setp plasmac.lowpass-frequency 0\n\n'\
                '#***** the joint associated with the Z axis *****\n')
            outFile.write('net plasmac:axis-position joint.{:d}.pos-fb => plasmac.axis-z-position\n\n'.format(self.zJoint))
            if self.arcVoltPin.get_text() and (self.mode == 0 or self.mode == 1):
                outFile.write('net plasmac:arc-voltage-in {} => plasmac.arc-voltage-in\n'.format(self.arcVoltPin.get_text()))
            if self.arcOkPin.get_text() and (self.mode == 1 or self.mode == 2):
                outFile.write('net plasmac:arc-ok-in {} => plasmac.arc-ok-in\n'.format(self.arcOkPin.get_text()))
            if self.floatPin.get_text():
                outFile.write('net plasmac:float-switch {} => debounce.0.0.in\n'.format(self.floatPin.get_text()))
            elif not self.floatPin.get_text():
                outFile.write('# net plasmac:float-switch {YOUR FLOAT SWITCH PIN} => debounce.0.0.in\n')
            if self.breakPin.get_text():
                outFile.write('net plasmac:breakaway {} => debounce.0.1.in\n'.format(self.breakPin.get_text()))
            elif not self.breakPin.get_text():
                outFile.write('# net plasmac:breakaway {YOUR BREAKAWAY PIN} => debounce.0.1.in\n')
            if self.ohmicInPin.get_text():
                outFile.write('net plasmac:ohmic-probe {} => debounce.0.2.in\n'.format(self.ohmicInPin.get_text()))
            elif not self.ohmicInPin.get_text():
                outFile.write('# net plasmac:ohmic-probe {YOUR OHMIC PROBE PIN} => debounce.0.2.in\n')
            if self.ohmicOutPin.get_text():
                outFile.write('net plasmac:ohmic-enable plasmac.ohmic-enable  => {}\n'.format(self.ohmicOutPin.get_text()))
            elif not self.ohmicOutPin.get_text():
                outFile.write('# net plasmac:ohmic-enable plasmac.ohmic-enable  => {YOUR OHMIC ENABLE PIN}\n')
            if self.torchPin.get_text():
                outFile.write('net plasmac:torch-on => {}\n'.format(self.torchPin.get_text()))
            if self.moveUpPin.get_text() and self.mode == 2:
                outFile.write('net plasmac:move-up {} => plasmac.move-up\n'.format(self.moveUpPin.get_text()))
            if self.moveDownPin.get_text() and self.mode == 2:
                outFile.write('net plasmac:move-down {} => plasmac.move-down\n'.format(self.moveDownPin.get_text()))
        return True

    def write_postgui_hal_file(self):
        # create a postgui.hal file if not already present
        if not os.path.exists('{}/postgui.hal'.format(self.newIniPath)):
            with open('{}/postgui.hal'.format(self.newIniPath), 'w') as outFile:
                outFile.write(\
                    '# Keep your post GUI customisations here to prevent them from\n'\
                    '# being overwritten by updates or pncconf/stepconf changes\n\n'\
                    '# As an example:\n'\
                    '# You may want to connect the plasmac components thc-enable pin\n'\
                    '# to a switch you have on your machine rather than let it be\n'\
                    '# controlled from the GUI Run tab.\n\n'\
                    '# First disconnect the GUI Run tab from the plasmac:thc-enable signal\n'\
                    '# net unlinkp plasmac_thc.enable-out\n\n'\
                    '# Then connect your switch input pint to the plasmac:thc-enable signal\n'\
                    '# net plasmac:thc-enable your.input-pin\n')
        return True

    def write_newini_file(self,display):
        # create a new INI file from the INI file copy and the plasmac INI file
        plasmacIni = open(self.plasmacIniFile,'r')
        outFile = open(self.newIniFile,'w')
        # comment out the test panel
        while 1:
            line = plasmacIni.readline()
            if line.startswith('# see notes'):
                pass
            elif line.startswith('[APPLICATIONS]') or \
                 line.startswith('DELAY') or \
                 line.startswith('APP'):
                outFile.write('# ' + line)
            elif not '[HAL]' in line:
                if line.startswith('MODE'):
                    outFile.write('MODE = {}\n'.format(self.mode))
                else:
                    outFile.write(line)
            else:
                break
        # add the [HAL] section
        outFile.write(\
            '[HAL]\n'\
            '# required\n'
            'TWOPASS = ON\n'\
            '# the base machine\n'\
            'HALFILE = {0}{1}\n'\
            '# the plasmac component connections\n'\
            'HALFILE = plasmac.hal\n'\
            '# the plasmac machine connections\n'\
            'HALFILE = {0}_connections.hal\n'\
            '# use this for customisation after GUI has loaded\n'\
            'POSTGUI_HALFILE = postgui.hal\n'\
            '# required\n'\
            'HALUI = halui\n'\
            '\n'\
            .format(self.machineName.lower(),self.halExt))
        # add the tabs to the display section
        inFile = open(self.readIniFile, 'r')
        while 1:        
            line = inFile.readline()
            if '[DISPLAY]' in line:
                outFile.write(line)
                break
        openFile = False
        while 1:
            line = inFile.readline()
            if not line.startswith('['):
                if line.startswith('OPEN_FILE'):
                    outFile.write('OPEN_FILE = \"\"\n')
                    openFile = True
                else:
                    outFile.write(line)
            else:
                inFile.close()
                break
        if not openFile and display == 'axis':
            outFile.write('OPEN_FILE = \"\"\n\n')
        while 1:        
            line = plasmacIni.readline()
            if line.startswith('EMBED'):
                outFile.write('# required\n' + line)
                break
        plasmaRun = False
        while 1:
            line = plasmacIni.readline()
            if not line.startswith('['):
                if display == 'axis' and self.panel:
                    if 'Plasma Run' in line:
                        outFile.write('#{}'.format(line))
                    elif 'plasmac_run_tab.glade' in line:
                        outFile.write('#{}'.format(line))
                    elif 'plasmac_run_panel.glade' in line:
                        outFile.write('GLADEVCP                = -c plasmac_run -u ./plasmac_run.py -H plasmac_run.hal plasmac_run_panel.glade\n')
                    else:
                        outFile.write(line)
                elif display == 'gmoccapy' and self.panel:
                    if 'Plasma Run' in line:
                        plasmaRun = True
                    if 'ntb_preview' in line and plasmaRun:
                        outFile.write('#{}'.format(line))
                        plasmaRun = False
                    elif 'plasmac_run_tab.glade' in line:
                        outFile.write('#{}'.format(line))
                    elif 'box_left' in line:
                        outFile.write('EMBED_TAB_LOCATION      = box_left\n')
                    elif 'plasmac_run_panel.glade' in line:
                        outFile.write('EMBED_TAB_COMMAND       = gladevcp -c plasmac_run -x {XID} -u ./plasmac_run.py -H plasmac_run.hal plasmac_run_panel.glade\n')
                    else:
                        outFile.write(line)
                else:
                    outFile.write(line)
            else:
                break
        if display == 'axis':
            outFile.write(\
                '# required\n'\
                'USER_COMMAND_FILE       = plasmac_axis.py\n\n')
        inFile.close()
        plasmacIni.close()
        done = ['[APPLICATIONS]',\
                '[PLASMAC]',\
                '[FILTER]',\
                '[RS274NGC]',\
                '[HAL]',\
                '[DISPLAY]']
        # iterate through INI file copy to get all missing sections
        inFile = open(self.readIniFile, 'r')
        validSection = True
        offsetAxis = False
        newName = False
        while 1:
            line = inFile.readline()
            if line.startswith('['):
                if line.strip() in done:
                    validSection = False
                else:
                    done.append(line.strip())
                    validSection = True
                if line.strip() == '[EMC]' and validSection:
                    newName = True
                elif line.strip() == '[AXIS_Z]' and validSection:
                    offsetAxis = True
                else:
                    newName = False
                    offsetAxis = False
            if validSection:
                if newName:
                    if line.startswith('MACHINE'):
                        outFile.write('MACHINE = {}\n'.format(self.machineName))
                    else:
                        outFile.write(line)
                elif offsetAxis:
                    if line.startswith('MAX_VELOCITY'):
                        outFile.write(
                            '# set to double the value in the corresponding joint\n'\
                            'MAX_VELOCITY = {}\n'.format(float(self.zVel) * 2))
                    elif line.startswith('MAX_ACCELERATION'):
                        outFile.write(\
                            '# set to double the value in the corresponding joint\n'\
                            'MAX_ACCELERATION = {}\n'\
                            '# shares the above two equally between the joint and the offset\n'\
                            'OFFSET_AV_RATIO = 0.5\n'.format(float(self.zAcc) * 2))
                    else:
                        outFile.write(line)
                else:
                    outFile.write(line)
            if not line:
                break
        outFile.close()
        inFile.close()
        return True

    def make_links(self,display):
        # make links plasmac application files in configuration directory
        for fileName in self.get_files_to_link(display):
            src = '{}/{}'.format(self.copyPath,fileName)
            if self.upgrade:
                dst = '{}/{}'.format(os.path.dirname(self.orgIniFile),fileName)
            else:
                dst = '{}/{}'.format(self.newIniPath,fileName)
            if os.path.islink(dst):
                os.unlink(dst)
            elif os.path.exists(dst):
                os.remove(dst)
            os.symlink(src,dst)
        return True

    def write_material_file(self):
        # create a new material file if not existing
        materialFile = '{}/{}_material.cfg'.format(self.newIniPath,self.machineName.lower())
        if os.path.exists(materialFile):
            return True
        else: # create a new material file if it doesn't exist
            with open(materialFile, 'w') as outFile:
                outFile.write(self.material_header())
        return True

    def print_success(self):
        self.dialog_ok(\
            'SUCCESS',\
            '\nConfiguration is complete.\n\n'\
            'You can run this configuration from a console as follows.\n\n'\
            'Full installation:\n'\
            'linuxcnc  '\
            '{0}/{1}/{1}.ini\n\n'\
            'Run In Place installation:\n'\
            '{2}/scripts/linuxcnc  '\
            '{0}/{1}/{1}.ini\n\n'\
            .format(self.configPath,self.machineName.lower(),self.gitPath))

    def create_widgets(self):
        self.VB = gtk.VBox()
        if not self.upgrade:
            self.modeVBox = gtk.VBox()
            self.modeHBox = gtk.HBox(homogeneous=True)
            self.mode0 = gtk.RadioButton(group=None, label='Mode: 0')
            self.modeHBox.pack_start(self.mode0)
            self.mode1 = gtk.RadioButton(group=self.mode0, label='Mode: 1')
            self.modeHBox.pack_start(self.mode1)
            self.mode2 = gtk.RadioButton(group=self.mode0, label='Mode: 2')
            self.modeHBox.pack_start(self.mode2)
            self.modeLabel = gtk.Label('Use arc voltage for both arc-OK and THC')
            self.modeLabel.set_alignment(0,0)
            modeBlank = gtk.Label('')
            self.modeVBox.pack_start(self.modeHBox)
            self.modeVBox.pack_start(self.modeLabel)
            self.modeVBox.pack_start(modeBlank)
            self.VB.pack_start(self.modeVBox,expand=False)
            self.nameVBox = gtk.VBox()
            nameLabel = gtk.Label('Machine Name:')
            nameLabel.set_alignment(0,0)
            self.nameFile = gtk.Entry()
            self.nameFile.set_width_chars(60)
            self.nameFile.set_tooltip_markup(\
                'The <b>name</b> of the new or existing machine.\n'\
                'If not existing, this creates a directory ~/linuxcnc/configs/<b>name</b>.\n'\
                '<b>name</b>.ini and <b>name</b>.hal are then written to this directory '\
                'as well as other required files and links to appplication files.\n\n')
            nameBlank = gtk.Label('')
            self.nameVBox.pack_start(nameLabel)
            self.nameVBox.pack_start(self.nameFile)
            self.nameVBox.pack_start(nameBlank)
            self.VB.pack_start(self.nameVBox,expand=False)
        self.iniVBox = gtk.VBox()
        if self.upgrade:
            self.iniLabel = gtk.Label('INI file of configuration to upgrade:')
        else:
            self.iniLabel = gtk.Label('INI file in existing working config:')
        self.iniLabel.set_alignment(0,0)
        self.iniFile = gtk.Entry()
        self.iniFile.set_width_chars(60)
        self.iniBlank = gtk.Label('')
        self.iniVBox.pack_start(self.iniLabel)
        self.iniVBox.pack_start(self.iniFile)
        self.iniVBox.pack_start(self.iniBlank)
        self.VB.pack_start(self.iniVBox,expand=False)
        if not self.upgrade:
            self.halVBox = gtk.VBox()
            halLabel = gtk.Label('HAL file in existing working config:')
            halLabel.set_alignment(0,0)
            self.halFile = gtk.Entry()
            self.halFile.set_width_chars(60)
            halBlank = gtk.Label('')
            self.halVBox.pack_start(halLabel)
            self.halVBox.pack_start(self.halFile)
            self.halVBox.pack_start(halBlank)
            self.VB.pack_start(self.halVBox,expand=False)
            self.arcVoltVBox = gtk.VBox()
            self.arcVoltLabel = gtk.Label('Arc Voltage HAL pin:')
            self.arcVoltLabel.set_alignment(0,0)
            self.arcVoltPin = gtk.Entry()
            self.arcVoltPin.set_width_chars(60)
            arcVoltBlank = gtk.Label('')
            self.arcVoltVBox.pack_start(self.arcVoltLabel)
            self.arcVoltVBox.pack_start(self.arcVoltPin)
            self.arcVoltVBox.pack_start(arcVoltBlank)
            self.VB.pack_start(self.arcVoltVBox,expand=False)
            self.arcOkVBox = gtk.VBox()
            self.arcOkLabel = gtk.Label('Arc OK HAL pin:')
            self.arcOkLabel.set_alignment(0,0)
            self.arcOkPin = gtk.Entry()
            self.arcOkPin.set_width_chars(60)
            arcOkBlank = gtk.Label('')
            self.arcOkVBox.pack_start(self.arcOkLabel)
            self.arcOkVBox.pack_start(self.arcOkPin)
            self.arcOkVBox.pack_start(arcOkBlank)
            self.VB.pack_start(self.arcOkVBox,expand=False)
            self.ohmicInVBox = gtk.VBox()
            ohmicInLabel = gtk.Label('Ohmic Probe HAL pin (optional):')
            ohmicInLabel.set_alignment(0,0)
            self.ohmicInPin = gtk.Entry()
            self.ohmicInPin.set_width_chars(60)
            ohmicInBlank = gtk.Label('')
            self.ohmicInVBox.pack_start(ohmicInLabel)
            self.ohmicInVBox.pack_start(self.ohmicInPin)
            self.ohmicInVBox.pack_start(ohmicInBlank)
            self.VB.pack_start(self.ohmicInVBox,expand=False)
            self.ohmicOutVBox = gtk.VBox()
            ohmicOutLabel = gtk.Label('Ohmic Probe Enable HAL pin (optional):')
            ohmicOutLabel.set_alignment(0,0)
            self.ohmicOutPin = gtk.Entry()
            self.ohmicOutPin.set_width_chars(60)
            ohmicOutBlank = gtk.Label('')
            self.ohmicOutVBox.pack_start(ohmicOutLabel)
            self.ohmicOutVBox.pack_start(self.ohmicOutPin)
            self.ohmicOutVBox.pack_start(ohmicOutBlank)
            self.VB.pack_start(self.ohmicOutVBox,expand=False)
            self.floatVBox = gtk.VBox()
            floatLabel = gtk.Label('Float Switch HAL pin (optional):')
            floatLabel.set_alignment(0,0)
            self.floatPin = gtk.Entry()
            self.floatPin.set_width_chars(60)
            floatBlank = gtk.Label('')
            self.floatVBox.pack_start(floatLabel)
            self.floatVBox.pack_start(self.floatPin)
            self.floatVBox.pack_start(floatBlank)
            self.VB.pack_start(self.floatVBox,expand=False)
            self.breakVBox = gtk.VBox()
            breakLabel = gtk.Label('Breakaway Switch HAL pin (optional):')
            breakLabel.set_alignment(0,0)
            self.breakPin = gtk.Entry()
            self.breakPin.set_width_chars(60)
            breakBlank = gtk.Label('')
            self.breakVBox.pack_start(breakLabel)
            self.breakVBox.pack_start(self.breakPin)
            self.breakVBox.pack_start(breakBlank)
            self.VB.pack_start(self.breakVBox,expand=False)
            self.torchVBox = gtk.VBox()
            torchLabel = gtk.Label('Torch On HAL pin:')
            torchLabel.set_alignment(0,0)
            self.torchPin = gtk.Entry()
            self.torchPin.set_width_chars(60)
            torchBlank = gtk.Label('')
            self.torchVBox.pack_start(torchLabel)
            self.torchVBox.pack_start(self.torchPin)
            self.torchVBox.pack_start(torchBlank)
            self.VB.pack_start(self.torchVBox,expand=False)
            self.moveUpVBox = gtk.VBox()
            self.moveUpLabel = gtk.Label('Move Up HAL pin:')
            self.moveUpLabel.set_alignment(0,0)
            self.moveUpPin = gtk.Entry()
            self.moveUpPin.set_width_chars(60)
            moveUpBlank = gtk.Label('')
            self.moveUpVBox.pack_start(self.moveUpLabel)
            self.moveUpVBox.pack_start(self.moveUpPin)
            self.moveUpVBox.pack_start(moveUpBlank)
            self.VB.pack_start(self.moveUpVBox,expand=False)
            self.moveDownVBox = gtk.VBox()
            self.moveDownLabel = gtk.Label('Move Down HAL pin:')
            self.moveDownLabel.set_alignment(0,0)
            self.moveDownPin = gtk.Entry()
            self.moveDownPin.set_width_chars(60)
            moveDownBlank = gtk.Label('')
            self.moveDownVBox.pack_start(self.moveDownLabel)
            self.moveDownVBox.pack_start(self.moveDownPin)
            self.moveDownVBox.pack_start(moveDownBlank)
            self.VB.pack_start(self.moveDownVBox,expand=False)
            self.tabPanelVBox = gtk.VBox()
            self.tabPanelHBox = gtk.HBox(homogeneous=True)
            self.tabPanel0 = gtk.RadioButton(group=None, label='Run Tab')
            self.tabPanelHBox.pack_start(self.tabPanel0)
            self.tabPanel1 = gtk.RadioButton(group=self.tabPanel0, label='Run Panel')
            self.tabPanelHBox.pack_start(self.tabPanel1)
            self.tabPanelLabel = gtk.Label('Run Frame is a tab behind preview')
            self.tabPanelLabel.set_alignment(0,0)
            tabPanelBlank = gtk.Label('')
            self.tabPanelVBox.pack_start(self.tabPanelHBox)
            self.tabPanelVBox.pack_start(self.tabPanelLabel)
            self.tabPanelVBox.pack_start(tabPanelBlank)
            self.VB.pack_start(self.tabPanelVBox,expand=False)
        BB = gtk.HButtonBox()
        if self.upgrade:
            self.create = gtk.Button('Upgrade')
        else:
            self.create = gtk.Button('Create')
        self.cancel = gtk.Button('Exit')
        BB.pack_start(self.create, True, True, 0)
        BB.pack_start(self.cancel, True, True, 0)
        BB.set_border_width(5)
        self.VB.pack_start(BB,expand=False)
        self.W.add(self.VB)
        self.W.show_all()
        if not self.upgrade:
            self.modeLabel.set_text('Use arc voltage for both arc-OK and THC')
            self.tabPanelLabel.set_text('Run Frame is a tab behind preview')
            self.arcVoltVBox.show()
            self.arcOkVBox.hide()
            self.moveUpVBox.hide()
            self.moveDownVBox.hide()
        else:
            self.W.set_title('PlasmaC Upgrader')

    def get_files_to_link(self,display):
        if display == 'axis':
            return ['imperial_startup.ngc',\
                    'M190',\
                    'materialverter.py',\
                    'metric_startup.ngc',\
                    'plasmac.hal',\
                    'plasmac_axis.py',\
                    'plasmac_config.glade',\
                    'plasmac_config.hal',\
                    'plasmac_config.py',\
                    'plasmac_gcode.py',\
                    'plasmac_run_panel.glade',\
                    'plasmac_run_tab.glade',\
                    'plasmac_run.hal',\
                    'plasmac_run.py',\
                    'plasmac_stats.glade',\
                    'plasmac_stats.hal',\
                    'plasmac_stats.py',\
                    'README.md',\
                    'tool.tbl',\
                    'test',\
                    ]
        elif display == 'gmoccapy':
            return ['imperial_startup.ngc',\
                    'M190',\
                    'materialverter.py',\
                    'metric_startup.ngc',\
                    'plasmac.hal',\
                    'plasmac_buttons.glade',\
                    'plasmac_buttons.hal',\
                    'plasmac_buttons.py',\
                    'plasmac_config.glade',\
                    'plasmac_config.hal',\
                    'plasmac_config.py',\
                    'plasmac_control.glade',\
                    'plasmac_control.hal',\
                    'plasmac_control.py',\
                    'plasmac_gcode.py',\
                    'plasmac_monitor.glade',\
                    'plasmac_monitor.hal',\
                    'plasmac_monitor.py',\
                    'plasmac_run_panel.glade',\
                    'plasmac_run_tab.glade',\
                    'plasmac_run.hal',\
                    'plasmac_run.py',\
                    'plasmac_stats.glade',\
                    'plasmac_stats.hal',\
                    'plasmac_stats.py',\
                    'README.md',\
                    'tool.tbl',\
                    'test',\
                    ]
        else:
            return None

    def material_header(self):
        return  '# plasmac material file\n'\
                '# the next line is required for version checking\n'\
                '[VERSION 1.1]\n\n'\
                '# example only, may be deleted\n'\
                '# items marked * are optional and will be 0 if not specified\n'\
                '# all other items are mandatory\n'\
                '#[MATERIAL_NUMBER_1]  \n'\
                '#NAME               = *\n'\
                '#KERF_WIDTH         = *\n'\
                '#THC                = * (0 = off, 1 = on)\n'\
                '#PIERCE_HEIGHT      = \n'\
                '#PIERCE_DELAY       = \n'\
                '#PUDDLE_JUMP_HEIGHT = *\n'\
                '#PUDDLE_JUMP_DELAY  = *\n'\
                '#CUT_HEIGHT         = \n'\
                '#CUT_SPEED          = \n'\
                '#CUT_AMPS           = * (only used for operator information)\n'\
                '#CUT_VOLTS          = * (modes 0 & 1 only, if not using auto voltage sampling)\n\n'

if __name__ == '__main__':
    try:
        a = configurator()
        gtk.main()
    except KeyboardInterrupt:
        pass
