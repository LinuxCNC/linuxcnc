#!/usr/bin/env python2

'''
configurator.py

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

import pygtk
pygtk.require('2.0')
import gtk
import os
import sys
import shutil
import linuxcnc

class configurator:

    def __init__(self):
        self.S = gtk.Window()
        self.S.set_title('PlasmaC Configurator')
        self.S.connect('delete_event', self.on_window_delete_event)
        self.S.set_position(gtk.WIN_POS_CENTER)
        SB = gtk.HButtonBox()
        self.new = gtk.Button('New')
        self.upg = gtk.Button('Upgrade')
        self.rec = gtk.Button('Reconfigure')
        self.can = gtk.Button(stock=gtk.STOCK_CLOSE)
        if 'configs/by_machine/plasmac' in os.path.realpath(os.path.dirname(sys.argv[0])):
            self.copyPath =  os.path.realpath(os.path.dirname(sys.argv[0]))
            self.S.set_default_size(240, 0)
            SB.pack_start(self.new, True, True, 0)
# ******************************************************************************
# remove this section when safe to do so
            SB.pack_start(self.upg, True, True, 0)
            SB.pack_start(self.rec, True, True, 0)
# ******************************************************************************
            SB.pack_end(self.can, True, True, 0)
        elif 'linuxcnc/configs' in os.path.realpath(os.path.dirname(sys.argv[0])):
            self.copyPath =  os.path.realpath(os.path.dirname(os.readlink('{}/configurator.py'.format(os.path.dirname(sys.argv[0])))))
            SB.pack_start(self.upg, True, True, 0)
            SB.pack_start(self.rec, True, True, 0)
            SB.pack_start(self.can, True, True, 0)
        else:
            print('Configurator started from unknown directory\n'\
                  'It must be located in a LinuxCNC configuration directory\n'\
                  'or a PlasmaC source directory')
            quit()
        self.configPath = os.path.expanduser('~') + '/linuxcnc/configs'
        SB.set_border_width(5)
        self.S.add(SB)
        self.S.show_all()
        self.new.connect('button_press_event', self.on_selection, 'new')
        self.upg.connect('button_press_event', self.on_selection, 'upgrade')
        self.rec.connect('button_press_event', self.on_selection, 'reconfigure')
        self.can.connect('button_press_event', self.on_selection, 'cancel')
        if len(sys.argv) == 3:
            if sys.argv[1] == 'upgrade':
                self.S.hide()
                self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
                self.on_selection('none', 'none', 'upgrade')
                self.on_create_clicked('none')

    def on_selection(self, button, event, selection):
        self.configureType = selection
        if len(sys.argv) == 3 and self.configureType == 'upgrade':
            self.orgIniFile = sys.argv[2]
            self.machineName = self.i.find('EMC', 'MACHINE')
            self.configDir = os.path.realpath(os.path.dirname(os.environ['INI_FILE_NAME']))
            return
        else:
            if self.configureType == 'new':
                result = self.dialog_ok_cancel(
                    'New Config Prerequisites',
                    '\nBefore using this configurator you should already have a\n'
                    'working configuration for your base machine.\n\n'
                    'The base machine should be fully operational.\n\n'
                    'If you don\'t have a working configuration then you need\n'
                    'to exit the configurator and create one.\n\n',
                    'Continue','Back')
                if not result: return
            elif self.configureType == 'upgrade':
                result = self.dialog_ok_cancel(\
                    'Upgrade Prerequisites',
                    '\nThis configurator will upgrade an existing plasmac configuration by:\n'
                    'Creating links to updated application files\n'
                    'Deleting links to redundant files\n'
                    'Making changes to the ini file if required\n\n'
                    'Existing machine HAL and custom HAL files will not be changed\n\n',
                    'Continue','Back')
                if not result: return
            elif self.configureType == 'reconfigure':
                result = self.dialog_ok_cancel(
                    'Reconfigure Prerequisites',
                    '\nThis configurator will enable the modifying of an\n'
                    'existing configuration.\n\n'
                    'If you don\'t have an existing configuration then you\n'
                    'need to exit the configurator and create one.\n\n',
                    'Continue','Back')
                if not result: return
            else:
                sys.exit()
        self.W = gtk.Window()
        self.W.connect('delete_event', self.on_window_delete_event)
        self.W.set_position(gtk.WIN_POS_CENTER)
        self.create_widgets()
        self.iniFile.connect('button_press_event', self.on_inifile_press_event)
        self.create.connect('clicked', self.on_create_clicked)
        self.cancel.connect('clicked', self.on_cancel_clicked)
        self.gitPath = self.copyPath.split('/config')[0]
        self.orgIniFile = ''
        self.configDir = ''
        if self.configureType == 'new':
            self.halFile.connect('button_press_event', self.on_halfile_press_event)
            self.nameFile.connect_after('focus-out-event', self.on_namefile_focus_out_event)
            self.nameFile.connect('changed', self.on_namefile_changed)
        if self.configureType == 'new' or self.configureType == 'reconfigure':
            self.mode0.connect('toggled', self.on_mode0_toggled)
            self.mode1.connect('toggled', self.on_mode1_toggled)
            self.mode2.connect('toggled', self.on_mode2_toggled)
            self.mode = 0
            self.tabPanel0.connect('toggled', self.on_tabPanel0_toggled)
            self.panel = 0
            self.newIniFile = ''
            self.orgHalFile = ''
#            self.plasmacIniFile = self.copyPath + '/metric_plasmac.ini'
            self.inPlace = False
            self.set_mode()

    def dialog_ok_cancel(self,title,text,name1,name2):
        dialog = gtk.Dialog(title,
                            self.S,
                            gtk.DIALOG_MODAL | gtk.DialogFlags.DESTROY_WITH_PARENT,
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
                    self.S,
                    gtk.DIALOG_MODAL | gtk.DialogFlags.DESTROY_WITH_PARENT,
                    (gtk.STOCK_OK, gtk.ResponseType.ACCEPT))
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
            self.set_mode()

    def on_mode1_toggled(self,button):
        if button.get_active():
            self.mode = 1
            self.set_mode()

    def on_mode2_toggled(self,button):
        if button.get_active():
            self.mode = 2
            self.set_mode()

    def on_tabPanel0_toggled(self,button):
        if button.get_active():
            self.panel = 0
            self.tabPanelLabel.set_text('Run Frame is a tab behind preview')
        else:
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
        self.configDir = '{}/{}'.format(self.configPath,self.machineName.lower())
        self.newIniFile = '{}/{}.ini'.format(self.configDir,self.machineName.lower())

    def on_inifile_press_event(self,button,event):
        self.dlg = gtk.FileChooserDialog('Open..', None, gtk.FILE_CHOOSER_ACTION_OPEN,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.ResponseType.OK))
        filter = gtk.FileFilter()
        filter.set_name('LinuxCNC INI File (*.ini)')
        filter.add_pattern('*.[Ii][Nn][Ii]')
        self.dlg.add_filter(filter)
        self.dlg.set_current_folder(self.configPath)
        if self.configureType == 'new':
            filter1 = gtk.FileFilter()
            filter1.set_name('All Files (*)')
            filter1.add_pattern('*')
            self.dlg.add_filter(filter1)
            if os.path.dirname(self.halFile.get_text()):
                self.dlg.set_current_folder(os.path.dirname(self.halFile.get_text()))
        response = self.dlg.run()
        if response == gtk.ResponseType.OK:
            self.iniFile.set_text(self.dlg.get_filename())
            self.orgIniFile = self.dlg.get_filename()
        else:
            self.dlg.destroy()
            self.iniFile.set_text('')
            self.orgIniFile = ''
            return
        if self.configureType == 'upgrade' or self.configureType == 'reconfigure':
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
        if self.configureType == 'reconfigure':
            self.modeSet = False
            self.populate_reconfigure()
        self.dlg.destroy()

    def on_halfile_press_event(self,button,event):
        self.dlg = gtk.FileChooserDialog('Save..', None, gtk.FILE_CHOOSER_ACTION_OPEN,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.ResponseType.OK))
        filter = gtk.FileFilter()
        filter.set_name('LinuxCNC HAL File (*.hal *.tcl)')
        filter.add_pattern('*.[Hh][Aa][Ll]')
        filter.add_pattern('*.[Tt][Cc][Ll]')
        self.dlg.add_filter(filter)
        filter1 = gtk.FileFilter()
        filter1.set_name('All Files (*)')
        filter1.add_pattern('*')
        self.dlg.add_filter(filter1)
        if os.path.dirname(self.iniFile.get_text()):
            self.dlg.set_current_folder(os.path.dirname(self.iniFile.get_text()))
        else:
            self.dlg.set_current_folder(self.configPath)
        response = self.dlg.run()
        if response == gtk.ResponseType.OK:
            self.halFile.set_text(self.dlg.get_filename())
            self.orgHalFile = self.dlg.get_filename()
            self.halExt = os.path.splitext(self.orgHalFile)[1]
        else:
            self.halFile.set_text('')
            self.orgHalFile = ''
        self.dlg.destroy()

    def on_cancel_clicked(self,button):
        self.W.hide()
        if len(sys.argv) == 3 and self.configureType == 'upgrade':
            sys.exit()

    def check_typos(self):
        with open(self.orgIniFile) as f:
            inData = f.readlines()
        for line in inData:
            if 'PAUSED-MOTION-SPEED' in line or 'TORCH-PULSE-TIME' in line:
                return True
        return False

    def fix_typos(self):
        shutil.copy(self.orgIniFile,'{}.tmp'.format(self.orgIniFile))
        inFile = open('{}.tmp'.format(self.orgIniFile), 'r')
        outFile = open('{}'.format(self.orgIniFile), 'w')
        for line in inFile:
            if line.startswith('PAUSED-MOTION-SPEED'):
                line = line.replace('-','_')
            elif line.startswith('TORCH-PULSE-TIME'):
                line = line.replace('-','_')
            outFile.write(line)
        inFile.close()
        outFile.close()
        os.remove('{}.tmp'.format(self.orgIniFile))

    def b4_pmt_tpt(self):
        inFile = open(self.orgIniFile,'r')
        while 1:
            line = inFile.readline()
            if line.startswith('[PLASMAC]'): break
        while 1:
            line = inFile.readline()
            if line.startswith('PAUSED_MOTION_SPEED'):
                inFile.close()
                return False
            elif line.startswith('[') or not line:
                inFile.close()
                return True

    def b4_scribe(self):
        inFile = open(self.orgIniFile,'r')
        while 1:
            line = inFile.readline()
            if line.startswith('[TRAJ]'): break
        while 1:
            line = inFile.readline()
            if line.startswith('SPINDLES'):
                inFile.close()
                return False
            elif line.startswith('[') or not line:
                inFile.close()
                return True

    def b4_spotting(self):
        inFile = open(self.orgIniFile,'r')
        while 1:
            line = inFile.readline()
            if line.startswith('[TRAJ]'): break
        while 1:
            line = inFile.readline()
            if line.startswith('SPINDLES'):
                if int(line.split('=')[1].strip()) >= 3:
                    inFile.close()
                    return False
            elif line.startswith('[') or not line:
                inFile.close()
                return True

    def b4_change_consumables(self):
        inFile = open(self.orgIniFile,'r')
        while 1:
            line = inFile.readline()
            if line.strip() == '[AXIS_X]': break
        while 1:
            line = inFile.readline()
            if line.startswith('OFFSET_AV_RATIO'):
                inFile.close()
                return False
            elif line.startswith('[') or not line:
                inFile.close()
                return True

    def b4_extras(self):
        inFile = open(self.orgIniFile,'r')
        while 1:
            line = inFile.readline()
            if line.strip() == '[PLASMAC]': break
        while 1:
            line = inFile.readline()
            if 'BUTTON_10' in line:
                inFile.close()
                return False
            elif line.startswith('[') or not line:
                inFile.close()
                return True

    def b4_pause_at_end(self):
        halFile = '{}/plasmac_run.hal'.format(self.configDir)
        if os.path.exists(halFile):
            inFile = open(halFile,'r')
            for line in inFile:
                if 'pause-at-end' in line:
                    inFile.close()
                    return False
            inFile.close()
            return True
        return False

    def b4_auto_upgrade(self):
        halFile = '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())
        inFile = open(halFile,'r')
        for line in inFile:
            if 'air-scribe' in line:
                inFile.close()
                return True
        inFile.close()
        return False

    def b4_pmx485(self):
        inFile = open(self.orgIniFile,'r')
        for line in inFile:
            if 'PM_PORT' in line:
                inFile.close()
                return False
        inFile.close()
        return True

    def b4_split_config_dir(self):
        if os.path.exists('{}/plasmac'.format(self.configDir)):
            if os.path.isdir('{}/plasmac'.format(self.configDir)):
                return False
        return True

    def b4_fix_config_dir_split(self):
        inFile = open(self.orgIniFile,'r')
        for line in inFile:
            if 'LAST_MAJOR_UPGRADE=0.144' in line.replace(' ',''):
                inFile.close()
                return False
        inFile.close()
        return True

    def set_mode(self):
        if self.mode == 0:
            self.modeLabel.set_text('Use arc voltage for both Arc-OK and THC')
            self.arcVoltVBox.show()
            self.arcOkVBox.hide()
            self.moveUpVBox.hide()
            self.moveDownVBox.hide()
        elif self.mode == 1:
            self.modeLabel.set_text('Use arc ok for Arc-OK and arc voltage for THC')
            self.arcVoltVBox.show()
            self.arcOkVBox.show()
            self.moveUpVBox.hide()
            self.moveDownVBox.hide()
        elif self.mode == 2:
            self.modeLabel.set_text('Use arc ok for Arc-OK and up/down signals for THC')
            self.arcVoltVBox.hide()
            self.arcOkVBox.show()
            self.moveUpVBox.show()
            self.moveDownVBox.show()

    # check existing version so we know what to upgrade
    def check_version(self):
        # ***********************************************************
        # *** set latestMajorUpgrade version number below         ***
        # *** set latestMajorUpgrade in plasmac_run.py            ***
        # *** set LAST_MAJOR_UPGRADE in upgrade_ini_file function ***
        # *** set LAST_MAJOR_UPGRADE in default_gui.init files    ***
        # *** set LAST_MAJOR_UPGRADE in all example .ini files    ***
        # *** set VERSION in plasmac.comp                         ***
        # *** set self.plasmacVersion in plasmac_config.py        ***
        # *** update versions.html                                ***
        # ***********************************************************
        self.latestMajorUpgrade = 0.144
        # see if this is a version before creating {MACHINE}_connections.hal
        if not os.path.exists('{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())):
            return 0.000
        # if version before changing paused_motion_speed and torch_pulse_time
        elif self.b4_pmt_tpt():
            return 0.010
        # if version before adding scribe
        elif self.b4_scribe():
            return 0.058
        # if version before adding spotting
        elif self.b4_spotting():
            return 0.066
        # if version before adding change consumables
        elif self.b4_change_consumables():
            return 0.074
        # if version before adding Extras panel
        elif self.b4_extras():
            return 0.079
        # if version before adding pause at end
        elif self.b4_pause_at_end():
            return 0.086
        # if version before automatic upgrades
        elif self.b4_auto_upgrade():
            return 0.089
        # if version before adding pmx485
        elif self.b4_pmx485():
            return 0.096
        # if version before splitting config directory
        elif self.b4_split_config_dir():
            return 0.139
        # if version before fixing the splitting of config directory
        elif self.b4_fix_config_dir_split():
            return 0.143
        # must be the latest version
        else:
        # *** set the latestMajorUpgrade version number in line 437 ***
            return self.latestMajorUpgrade

    def on_create_clicked(self,button):
        if len(sys.argv) == 3 and self.configureType == 'upgrade':
            pass
        else:
            if not self.check_entries():
                self.W.present()
                return
            if self.configureType == 'reconfigure':
                self.reconfigure()
                self.W.hide()
                self.dialog_ok('RECONFIGURE','\nReconfigure is complete.\n\n')
                return
            if not self.check_new_path(): return
        display = self.get_display()
        if display == None: return
        if self.configureType == 'upgrade':
            if self.check_typos():
                self.fix_typos()
            versionMajor = self.check_version()
            if versionMajor > 0.139:
                plasmacPath = '/plasmac'
            else:
                plasmacPath = ''
            try:
                with open('{}{}/plasmac_config.py'.format(self.configDir, plasmacPath), 'r') as verFile:
                    for line in verFile:
                        if 'self.plasmacVersion =' in line:
                            self.versionCurrent = float(line.split('PlasmaC v')[1].replace('\'',''))
                            break
            except:
                self.versionCurrent = 0.0
            if versionMajor == self.latestMajorUpgrade:
                self.make_links(display, versionMajor)
                with open('{}/plasmac/plasmac_config.py'.format(self.configDir), 'r') as verFile:
                    for line in verFile:
                        if 'self.plasmacVersion =' in line:
                            self.versionNew = float(line.split('PlasmaC v')[1].replace('\'',''))
                            break
                if float(self.versionNew) > float(self.versionCurrent):
                    if self.versionCurrent > 0:
                        msg = '\nUpgraded from v{:0.3f} to v{:0.3f}\n'.format(self.versionCurrent, self.versionNew)
                    else:
                        msg = '\nUpgraded from unknown version to v{:0.3f}\n'.format( self.versionNew)
                else:
                    msg = '\nUpgrade not required.\n\nv{:0.3f} is the latest version\n'.format(self.versionCurrent)
                if len(sys.argv) == 3 and self.configureType == 'upgrade':
                    msg = '\nPlasmaC automatic upgrade has failed.\n\n'
                    msg += 'Check for the correct version number in:\n\n'
                    msg += '{}\n\n'.format(self.orgIniFile)
                    self.dialog_ok('FAILURE', msg)
                    sys.exit()
                self.dialog_ok('UPGRADE', msg)
                sys.exit()
                return
            if versionMajor < 0.140:
                # make sure no plasmac file exists
                if os.path.exists('{}/plasmac'.format(self.configDir)):
                    if not os.path.isdir('{}/plasmac'.format(self.configDir)):
                        os.rename('{}/plasmac'.format(self.configDir), '{}/plasmac.001'.format(self.configDir))
                # create backups dir if required
                if not os.path.exists('{}/backups'.format(self.configDir)):
                    os.makedirs('{}/backups'.format(self.configDir))
            self.upgrade_ini_file(versionMajor,display)
            self.upgrade_material_file(versionMajor)
            self.upgrade_connections_file(versionMajor)
            self.upgrade_config_files(versionMajor)
            self.make_links(display, versionMajor)
            with open('{}/plasmac/plasmac_config.py'.format(self.configDir), 'r') as verFile:
                for line in verFile:
                    if 'self.plasmacVersion =' in line:
                        self.versionNew = float(line.split('PlasmaC v')[1].replace('\'',''))
                        break
            if self.versionCurrent > 0:
                upg = '\nUpgraded from v{:0.3f} to v{:0.3f}\n'.format(self.versionCurrent, self.versionNew)
            else:
                upg = '\nUpgraded from unknown version to v{:0.3f}\n'.format(self.versionNew)
            if versionMajor < 0.140:
            # move old backup files to backups dir
                for file in os.listdir(self.configDir):
                    if '_original' in file or 'cfg.old' in file or 'hal.old' in file or 'ini.old' in file:
                        shutil.move(os.path.join(self.configDir,file), os.path.join(self.configDir,'backups/{}'.format(file)))
                    elif '.pyc' in file:
                        os.remove(os.path.join(self.configDir,file))
            if len(sys.argv) == 3:
                msg = '\nPlasmaC has been automatically upgraded.\n'
                msg += upg
                msg += '\nLinuxCNC will need to be restarted.\n\n'
            else:
                self.W.hide()
                msg = '\nPlasmaC Upgrade has completed.\n'
                msg += '{}\n\n'.format(upg)
            self.dialog_ok('UPGRADE', msg)
            print(msg)
            sys.exit()
            return
        if not self.copy_ini_and_hal_files(): return
        if not self.get_traj_info(self.readIniFile,display): return
        if not self.get_kins_info(self.readIniFile): return
        if not self.write_new_hal_file(): return
        if not self.write_connections_hal_file(): return
        if not self.write_postgui_hal_file(): return
        if not self.write_newini_file(display): return
        if not self.make_links(display, 'dummy'): return
        with open('{}/plasmac/plasmac_config.py'.format(self.configDir), 'r') as verFile:
            for line in verFile:
                if 'self.plasmacVersion =' in line:
                    self.versionNew = float(line.split('PlasmaC v')[1].replace('\'',''))
                    break
        print('\nInstalled PlasmaC v{:0.3f}\n'.format(self.versionNew))
        if not self.write_material_file(): return
        self.W.hide()
        self.success_dialog()

    def check_entries(self):
        # check if entries are valid
        if self.configureType == 'upgrade':
            if not self.iniFile.get_text():
                self.dialog_ok('ERROR','INI file is required')
                return False
        else:
            if self.configureType == 'new':
                if not self.nameFile.get_text():
                    self.dialog_ok('ERROR','Machine name is required')
                    return False
                if not self.halFile.get_text():
                    self.dialog_ok('ERROR','HAL file is required')
                    return False
            if not self.iniFile.get_text():
                self.dialog_ok('ERROR','INI file is required')
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
        if self.configureType == 'new':
            if not os.path.exists('{}/backups'.format(self.configDir)):
                os.makedirs('{}/backups'.format(self.configDir))
            else:
                if not self.dialog_ok_cancel('CONFIGURATION EXISTS',\
                                             '\nA configuration already exists in {}\n'\
                                             .format(self.configDir),'Overwrite','Back'):
                    return False
        return True

    def upgrade_connections_file(self,versionMajor):
        # add a connections.hal file for an upgrade from 0.0
        if versionMajor == 0.000:
            inFile = open('{}/plasmac.hal'.format(os.path.dirname(self.orgIniFile)), 'r')
            outFile = open('{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()), 'w')
            outFile.write(\
                '# Keep your plasmac i/o connections here to prevent them from\n'\
                '# being overwritten by updates or pncconf/stepconf changes\n\n'\
                '# Other customisations may be placed here as well\n'\
                '# This file is built by the configurator in your configuration directory\n\n')
            for line in inFile:
                if ' '.join(line.split()).startswith('loadrt debounce'):
                    outFile.write(\
                        '#***** debounce for the float, ohmic and breakaway switches *****\n'\
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
                        'setp plasmac.lowpass-frequency 0\n\n'\
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
        # add scribe for an upgrade from 0.058 or earlier
        if versionMajor < 0.059:
            conFile = '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())
            if os.path.exists(conFile):
                shutil.copy(conFile,'{}.old058'.format(conFile))
                inFile = open('{}.old058'.format(conFile), 'r')
                outFile = open(conFile, 'w')
                todo = False
                while 1:
                    line = inFile.readline()
                    if not line or line.strip() == '':
                        if todo:
                            outFile.write('\n# a 1 here allows multiple tools to be used\n' \
                                            '# gcode M3 S1 needs to be changed to:\n' \
                                            '# M3 $0 S1 for the plasma torch\n' \
                                            '# M3 $1 S1 for the scribe\n' \
                                            'setp plasmac.multi-tool 0\n')
                            outFile.write('# net plasmac:scribe-arm plasmac.scribe-arm => ***YOUR_SCRIBE_ARMING_OUTPUT***\n')
                            outFile.write('# net plasmac:scribe-on  plasmac.scribe-on  => ***YOUR_SCRIBE_ON_OUTPUT***\n')
                            outFile.write(line)
                            todo = False
                        elif not line:
                            break
                        else:
                            outFile.write(line)
                    elif 'torch-on' in line:
                        outFile.write(line)
                        todo = True
                    else:
                        outFile.write(line)
                inFile.close()
                outFile.close()
            else:
                print('No connections file to upgrade')
        # add spotting for an upgrade from 0.066 or earlier
        if versionMajor < 0.067:
            conFile = '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())
            if os.path.exists(conFile):
                shutil.copy(conFile,'{}.old066'.format(conFile))
                inFile = open('{}.old066'.format(conFile), 'r')
                outFile = open(conFile, 'w')
                for line in inFile:
                    if '# M3 $1 S1 for the scribe' in line:
                        outFile.write(line)
                        outFile.write('# M3 $2 S1 for spotting\n')
                    else:
                        outFile.write(line)
        # add automatic upgrades from 0.089 or earlier
        if versionMajor < 0.090:
            conFile = '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())
            if os.path.exists(conFile):
                shutil.copy(conFile,'{}.old089'.format(conFile))
                inFile = open('{}.old089'.format(conFile), 'r')
                outFile = open(conFile, 'w')
                for line in inFile:
                    if 'air scribe' in line:
                        outFile.write(line.replace('air scribe', 'scribe'))
                    elif 'air-scribe-arm' in line:
                        outFile.write(line.replace('air-scribe-arm', 'scribe-arm    ').replace('AIR_', ''))
                    elif 'air-scribe-start' in line:
                        outFile.write(line.replace('air-scribe-start', 'scribe-on       ').replace('AIR_SCRIBE_START', 'AIR_SCRIBE_ON'))
                    else:
                        outFile.write(line)
                inFile.close()
                outFile.close()

    def upgrade_config_files(self,versionMajor):
        if versionMajor < 0.090:
            # add automatic upgrades from 0.089 or earlier
            cfgFile = '{}/{}_config.cfg'.format(self.configDir,self.machineName.lower())
            if os.path.exists(cfgFile):
                shutil.copy(cfgFile,'{}.old089'.format(cfgFile))
                inFile = open('{}.old089'.format(cfgFile), 'r')
                outFile = open(cfgFile, 'w')
                for line in inFile:
                    if 'centre-spot-threshold' in line:
                        outFile.write(line.replace('centre-spot-threshold', 'spotting-threshold'))
                    elif 'centre-spot-time' in line:
                        outFile.write(line.replace('centre-spot-time', 'spotting-time'))
                    elif 'scribe-start-delay' in line:
                        outFile.write(line.replace('scribe-start-delay', 'scribe-on-delay'))
                    else:
                        outFile.write(line)
                inFile.close()
                outFile.close()

    def upgrade_ini_file(self,versionMajor,display):
        # add a connections.hal file for an upgrade from 0.0
        if versionMajor == 0.000:
            shutil.copy(self.orgIniFile,'{}.old000'.format(self.orgIniFile))
            inFile = open('{}.old000'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if ''.join(line.split()) == 'HALFILE=plasmac.hal':
                    outFile.write(\
                        'HALFILE                 = plasmac.hal\n'\
                        '# the plasmac machine connections\n'\
                        'HALFILE                 = {}_connections.hal\n'\
                        .format(self.machineName.lower()))
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        # add paused-motion-time and torch-pulse-time for an upgrade from 0.010 or earlier
        # add choice of run tab or run panel for an upgrade from 0.010 or earlier
        if versionMajor < 0.011:
            shutil.copy(self.orgIniFile,'{}.old010'.format(self.orgIniFile))
            inFile = open('{}.old010'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if line.startswith('# multiply cut-fe'): pass
                elif line.startswith('PAUSED_MOTION'): pass
                elif line.startswith('# torch on time'): pass
                elif line.startswith('TORCH_PULSE_TIM'): pass
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
        # add scribe for an upgrade from 0.058 or earlier
        if versionMajor < 0.059:
            shutil.copy(self.orgIniFile,'{}.old058'.format(self.orgIniFile))
            inFile = open('{}.old058'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            displaySect = False
            tooledit = False
            for line in inFile:
                if line.startswith('[TRAJ]'):
                    outFile.write(line)
                    outFile.write('SPINDLES = 2\n')
                elif line.startswith('HALFILE'):
                    outFile.write(line)
                    halFile = line.split('=')[1].strip()
                    if halFile != 'plasmac.hal' and 'connections.hal' not in line:
                        self.add_spindle_to_halfile(halFile)
                elif line.startswith('[DISPLAY]') and display == 'axis':
                    outFile.write(line)
                    displaySect = True
                elif line.startswith('TOOL_EDITOR') and displaySect:
                    outFile.write('TOOL_EDITOR = tooledit x y\n')
                    tooledit = True
                elif (line.startswith('[') or line.strip() == '') and displaySect and not tooledit:
                    outFile.write('TOOL_EDITOR = tooledit x y\n\n')
                    outFile.write(line)
                    tooledit = True
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        # add spotting for an upgrade from 0.066 or earlier
        if versionMajor < 0.067:
            shutil.copy(self.orgIniFile,'{}.old066'.format(self.orgIniFile))
            inFile = open('{}.old066'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            traj = False
            for line in inFile:
                if line.startswith('SPINDLES'):
                    outFile.write('SPINDLES = 3\n')
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        # add change consumables for an upgrade from 0.074 or earlier
        if versionMajor < 0.075:
            shutil.copy(self.orgIniFile,'{}.old074'.format(self.orgIniFile))
            inFile = open('{}.old074'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            axis = False
            for line in inFile:
                if line.startswith('# there are three'):
                    outFile.write('# there are four special commands:\n')
                elif line.startswith('# probe-test, ohmic-test'):
                    outFile.write('# probe-test, ohmic-test, cut-type and change-consumables\n')
                elif line.startswith('# cut-type switches'):
                    outFile.write(line)
                    outFile.write('# change-consumables moves the torch to the specified machine coordinates when paused\n'\
                                  '# e.g. change-consumables x10 f1000 will move to X10 at 1000 units per minute\n'\
                                  '# valid entries are Xnnn Ynnn Fnnn. F is mandatory and at least one of X or Y are required\n')
                elif line.strip() == '[AXIS_X]' or line.strip() == '[AXIS_Y]':
                    outFile.write(line)
                    axis = True
                elif line.startswith('['):
                    outFile.write(line)
                    axis = False
                elif axis and line.startswith('MAX_VELOCITY'):
                    a,b = line.strip().replace(' ','').split('=')
                    outFile.write(
                        '# set to double the value in the corresponding joint\n'\
                        'MAX_VELOCITY = {}\n'.format(float(b) * 2))
                elif axis and line.startswith('MAX_ACCELERATION'):
                    a,b = line.strip().replace(' ','').split('=')
                    outFile.write(
                        '# set to double the value in the corresponding joint\n'\
                        'MAX_ACCELERATION = {}\n'\
                        '# shares the above two equally between the joint and the offset\n'\
                        'OFFSET_AV_RATIO = 0.5\n'.format(float(b) * 2))
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        # add Extras panel for an upgrade from 0.079 or earlier
        if versionMajor < 0.080:
            shutil.copy(self.orgIniFile,'{}.old079'.format(self.orgIniFile))
            inFile = open('{}.old079'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            buttons = [['',''],['',''],['',''],['',''],['','']]
            buttons_do = False
            extras = False
            for line in inFile:
                if line.startswith('TORCH_PULSE_TIME'):
                    outFile.write('{}\n'.format(line))
                    buttons_do = True
                elif buttons_do:
                    if line.startswith('BUTTON_'):
                        n = int(filter(str.isdigit, line.split('=')[0])) - 1
                        if (display == 'axis' and n < 5) or (display == 'gmoccapy' and n < 4):
                            if '_NAME' in line:
                                buttons[n][0] = line
                            elif '_CODE' in line:
                                buttons[n][1] = line
                        else:
                            if display == 'axis':
                                print('Limit of 5 user buttons:\n"{}" is invalid\n'.format(line.strip()))
                            elif display == 'gmoccapy':
                                print('Limit of 4 user buttons:\n"{}" is invalid\n'.format(line.strip()))
                    elif 'removing z axis moves' in line:
                        if display == 'axis':
                            outFile.write('# for the five user buttons in the main window\n')
                        elif display == 'gmoccapy':
                            outFile.write('# for the four user buttons in the main window\n')
                        for n in range(5):
                            outFile.write(buttons[n][0])
                            outFile.write(buttons[n][1])
                        outFile.write('\n'\
                            '# for the ten user buttons in the Extras panel\n'\
                            'BUTTON_10_NAME           = \nBUTTON_10_CODE           = \nBUTTON_10_IMAGE          = \n'\
                            'BUTTON_11_NAME           = \nBUTTON_11_CODE           = \nBUTTON_11_IMAGE          = \n'\
                            'BUTTON_12_NAME           = \nBUTTON_12_CODE           = \nBUTTON_12_IMAGE          = \n'\
                            'BUTTON_13_NAME           = \nBUTTON_13_CODE           = \nBUTTON_13_IMAGE          = \n'\
                            'BUTTON_14_NAME           = PlasmaC\User Guide\n'\
                            'BUTTON_14_CODE           = %xdg-open http://linuxcnc.org/docs/devel/html/plasma/plasmac-user-guide.html\n'\
                            'BUTTON_14_IMAGE          = \n'\
                            'BUTTON_15_NAME           = \nBUTTON_15_CODE           = \nBUTTON_15_IMAGE          = \n'\
                            'BUTTON_16_NAME           = \nBUTTON_16_CODE           = \nBUTTON_16_IMAGE          = \n'\
                            'BUTTON_17_NAME           = \nBUTTON_17_CODE           = \nBUTTON_17_IMAGE          = \n'\
                            'BUTTON_18_NAME           = \nBUTTON_18_CODE           = \nBUTTON_18_IMAGE          = \n'\
                            'BUTTON_19_NAME           = LinuxCNC\Docs\n'\
                            'BUTTON_19_CODE           = %xdg-open http://linuxcnc.org/docs/devel/html\n'\
                            'BUTTON_19_IMAGE          = \n\n')
                        outFile.write(line)
                        buttons_do = False
                elif line.startswith('DISPLAY'):
                    outFile.write('{}\n'.format(line))
                    extras = True
                elif line.startswith('EMBED_TAB_NAME') and extras:
                    if line.split('=')[1].strip() == 'Plasma Run':
                        outFile.write('EMBED_TAB_NAME          = Run\n')
                    elif line.split('=')[1].strip() == 'Plasma Config':
                        outFile.write('EMBED_TAB_NAME          = Config\n')
                    else:
                        outFile.write(line)
                elif line.startswith('[') and extras:
                    if display == 'axis':
                        outFile.write(\
                            'EMBED_TAB_NAME          = Extras\n'\
                            'EMBED_TAB_COMMAND       = halcmd loadusr -Wn plasmac_wizards gladevcp -c plasmac_wizards -x {XID} -u ./plasmac_wizards.py plasmac_wizards.glade\n\n')
                    elif display == 'gmoccapy':
                        outFile.write(\
                            'EMBED_TAB_NAME          = Extras\n'\
                            'EMBED_TAB_LOCATION      = ntb_preview\n'\
                            'EMBED_TAB_COMMAND       = halcmd loadusr -Wn plasmac_wizards gladevcp -c plasmac_wizards -x {XID} -u ./plasmac_wizards.py plasmac_wizards.glade\n\n')
                    outFile.write(line)
                    extras = False
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        #add pause at end for an upgrade from 0.087 or earlier
        if versionMajor < 0.088:
            shutil.copy(self.orgIniFile,'{}.old087'.format(self.orgIniFile))
            inFile = open('{}.old087'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if ''.join(line.split()) == 'HALFILE=plasmac.hal':
                    outFile.write('HALFILE                 = plasmac.tcl\n')
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        # add automatic upgrades from 0.089 or earlier
        if versionMajor < 0.090:
            shutil.copy(self.orgIniFile,'{}.old089'.format(self.orgIniFile))
            inFile = open('{}.old089'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if line.startswith('[PLASMAC]'):
                    outFile.write(line)
                    outFile.write('\n# required for upgrades (DO NOT CHANGE)\n')
                    outFile.write('LAST_UPGRADE            = 0.090\n')
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()
        # add powermax comms for an upgrade from 0.096 or earlier
        if versionMajor < 0.097:
            shutil.copy(self.orgIniFile,'{}.old096'.format(self.orgIniFile))
            inFile = open('{}.old096'.format(self.orgIniFile), 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if line.startswith('LAST_UPGRADE'):
                    outFile.write('LAST_UPGRADE            = 0.097\n')
                elif line.startswith('TORCH_PULSE_TIME'):
                    outFile.write(line)
                    outFile.write('\n# for Powermax communications\n')
                    outFile.write('#PM_PORT                 = /dev/ttyUSB0\n')
                    outFile.write('#PM_PRESSURE_DISPLAY     = Bar\n')
                else:
                    outFile.write(line)
            inFile.close()
            outFile.close()

        if versionMajor < 0.140:
            bkpFile = '{}/backups/{}.old139'.format(self.configDir, os.path.basename(self.orgIniFile))
            shutil.copy(self.orgIniFile,bkpFile)
            inFile = open(bkpFile, 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if line.startswith('LAST_UPGRADE') or line.startswith('LAST_MAJOR_UPGRADE'):
                    line = 'LAST_MAJOR_UPGRADE      = 0.140\n'
                elif './test/plasmac_' in line and not './plasmac' in line:
                    line = line.replace('./test/plasmac_', './plasmac/test/plasmac_')
                elif './plasmac_gcode' in line:
                    line = line.replace('./plasmac_gcode', './plasmac/plasmac_gcode')
                elif 'SUBROUTINE_PATH' in line and not './plasmac' in line:
                    line = line.replace('./:', './:./plasmac:')
                elif 'USER_M_PATH' in line and not './plasmac' in line:
                    line = line.replace(' ./', ' ./:./plasmac')
                elif 'plasmac.tcl' in line and not './plasmac' in line:
                    line = line.replace('plasmac.tcl', './plasmac/plasmac.tcl')
                elif 'plasmac_axis.py' in line and not './plasmac' in line:
                    line = line.replace('plasmac_axis.py', './plasmac/plasmac_axis.py')
                elif 'EMBED_TAB_COMMAND' in line:
                    if 'plasmac_buttons' in line:
                        line = 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_buttons -x {XID} -u ./plasmac/plasmac_buttons.py -H ./plasmac/plasmac_buttons.hal ./plasmac/plasmac_buttons.glade\n'
                    elif 'plasmac_control' in line:
                        line = 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_control -x {XID} -u ./plasmac/plasmac_control.py -H ./plasmac/plasmac_control.hal ./plasmac/plasmac_control.glade\n'
                    elif 'plasmac_stats' in line:
                        line = 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_stats -x {XID} -u ./plasmac/plasmac_stats.py -H ./plasmac/plasmac_stats.hal ./plasmac/plasmac_stats.glade\n'
                    elif 'plasmac_run' in line and 'run_tab' in line:
                        line = '#' if line.startswith('#') else ''
                        line += 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_run -x {XID} -u ./plasmac/plasmac_run.py -H ./plasmac/plasmac_run.hal ./plasmac/plasmac_run_tab.glade\n'
                    elif 'plasmac_run' in line and 'run_panel' in line:
                        line = '#' if line.startswith('#') else ''
                        line += 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_run -x {XID} -u ./plasmac/plasmac_run.py -H ./plasmac/plasmac_run.hal ./plasmac/plasmac_run_panel.glade\n'
                    elif 'plasmac_config' in line:
                        line = 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_config -x {XID} -u ./plasmac/plasmac_config.py -H ./plasmac/plasmac_config.hal ./plasmac/plasmac_config.glade\n'
                    elif 'plasmac_monitor' in line:
                        line = 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_monitor -x {XID} -u ./plasmac/plasmac_monitor.py -H ./plasmac/plasmac_monitor.hal ./plasmac/plasmac_monitor.glade\n'
                    elif 'plasmac_wizards' in line:
                        line = 'EMBED_TAB_COMMAND       = gladevcp -c plasmac_wizards -x {XID} -u ./plasmac/plasmac_wizards.py ./plasmac/plasmac_wizards.glade\n'
                elif 'GLADEVCP' in line:
                    if 'plasmac_run' in line:
                        line = '#' if line.startswith('#') else ''
                        line += 'GLADEVCP                = -c plasmac_run -u ./plasmac/plasmac_run.py -H ./plasmac/plasmac_run.hal ./plasmac/plasmac_run_panel.glade\n'
                outFile.write(line)
            inFile.close()
            outFile.close()

        if versionMajor < 0.144:
            bkpFile = '{}/backups/{}.old143'.format(self.configDir, os.path.basename(self.orgIniFile))
            shutil.copy(self.orgIniFile,bkpFile)
            inFile = open(bkpFile, 'r')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            for line in inFile:
                if line.startswith('LAST_UPGRADE') or line.startswith('LAST_MAJOR_UPGRADE'):
                    line = 'LAST_MAJOR_UPGRADE      = 0.144\n'
                elif './plasmac/test/plasmac_' in line:
                    line = line.replace('./plasmac/test/plasmac_', 'test/plasmac_')
                outFile.write(line)
            inFile.close()
            outFile.close()

    def upgrade_material_file(self,versionMajor):
        materialFile = '{}/{}_material.cfg'.format(self.configDir,self.machineName.lower())
        if os.path.exists(materialFile):
            inFile = open(materialFile, 'r')
            mVersion = 0
            for line in inFile:
                if '[VERSION' in line:
                    mVersion = round(float(line.strip().strip(']').split(' ')[1]), 3)
            inFile.close()
            # change material file format
            if mVersion == 1.0:
                shutil.copy(materialFile,'{}.old010'.format(materialFile))
                inFile = open('{}.old010'.format(materialFile), 'r')
                outFile = open(materialFile, 'w')
                outFile.write(self.material_header())
                while 1:
                    line = inFile.readline()
                    if line.startswith('[MATERIAL_NUMBER'):
                        outFile.write(line)
                        break
                    if not line:
                        inFile.close()
                        outFile.close()
                        break
                while 1:
                    line = inFile.readline()
                    if not line:
                        inFile.close()
                        break
                    outFile.write(line)
                inFile.close()
                outFile.close()
            #add pause at end for an upgrade from 0.087 or earlier
            if versionMajor < 0.088:
                shutil.copy(materialFile,'{}.old087'.format(materialFile))
                inFile = open('{}.old087'.format(materialFile), 'r')
                outFile = open(materialFile, 'w')
                outFile.write(self.material_header())
                while 1:
                    line = inFile.readline()
                    if line.startswith('[MATERIAL_NUMBER'):
                        outFile.write(line)
                        break
                    if not line:
                        inFile.close()
                        outFile.close()
                        break
                while 1:
                    line = inFile.readline()
                    if not line:
                        inFile.close()
                        outFile.close()
                        break
                    elif line.startswith('CUT_VOLTS'):
                        outFile.write(line)
                        outFile.write('PAUSE_AT_END       = 0\n')
                    elif line.startswith('[VERSION'):
                        pass
                    else:
                        outFile.write(line)
            #add powermax comms for an upgrade from 0.096 or earlier
            if versionMajor < 0.097:
                shutil.copy(materialFile,'{}.old096'.format(materialFile))
                inFile = open('{}.old096'.format(materialFile), 'r')
                outFile = open(materialFile, 'w')
                outFile.write(self.material_header())
                while 1:
                    line = inFile.readline()
                    if line.startswith('[MATERIAL_NUMBER'):
                        outFile.write(line)
                        break
                    if not line:
                        inFile.close()
                        outFile.close()
                        return
                while 1:
                    line = inFile.readline()
                    if not line:
                        inFile.close()
                        break
                    elif line.startswith('PAUSE_AT_END'):
                        outFile.write(line)
                        outFile.write('GAS_PRESSURE       = 0\n')
                        outFile.write('CUT_MODE           = 1\n')
                    else:
                        outFile.write(line)
                inFile.close()
                outFile.close()
        else:
            print('No material file to upgrade')

    def add_spindle_to_halfile(self,halfile):
        halFile = '{}/{}'.format(self.configDir,halfile)
        toolCange = False
        if os.path.exists(halFile):
            shutil.copy(halFile,'{}.old058'.format(halFile))
            inFile = open('{}.old058'.format(halFile), 'r')
            outFile = open(halFile, 'w')
            for line in inFile:
                if line.replace(' ','').startswith('loadrtmotmod') or line.replace(' ','').startswith('loadrt[EMCMOT]EMCMOT'):
                    if 'num_spindles' in line:
                        line = line.split('num_spindles')[0].strip()
                    line = '{} num_spindles=[TRAJ]SPINDLES\n'.format(line.strip())
                elif 'hal_manualtoolchange' in line or 'iocontrol.0.tool' in line:
                    line = '# {}'.format(line)
                outFile.write(line)
            outFile.write(\
                '\n# toolchange passthrough\n'
                'net tool:change iocontrol.0.tool-change  => iocontrol.0.tool-changed\n'
                'net tool:prep   iocontrol.0.tool-prepare => iocontrol.0.tool-prepared\n')
            inFile.close()
            outFile.close()
        else:
            print('No hal file to upgrade')

    def copy_ini_and_hal_files(self):
        # copy original INI and HAL files for input and backup
        if os.path.dirname(self.orgIniFile) == '{}/backups'.format(self.configDir) and \
           os.path.basename(self.orgIniFile).startswith('_original_'):
            self.readIniFile = self.orgIniFile
        else:
            self.readIniFile = '{}/backups/_original_{}'.format(self.configDir,os.path.basename(self.orgIniFile))
            shutil.copyfile(self.orgIniFile,self.readIniFile)

        if os.path.dirname(self.orgHalFile) == '{}/backups'.format(self.configDir) and \
           os.path.basename(self.orgHalFile).startswith('_original_'):
            self.readHalFile = self.orgHalFile
        else:
            self.readHalFile = '{}/backups/_original_{}'.format(self.configDir,os.path.basename(self.orgHalFile))
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
                self.W.destroy()
                return False
        result = 0
        while 1:
            line = inFile.readline()
            if 'LINEAR_UNITS' in line:
                result += 1
                a,b = line.strip().replace(' ','').split('=')
                if b.lower() == 'inch':
                    self.plasmacIniFile = '{}/default_{}_imperial.init'.format(self.copyPath, display)
                else:
                    self.plasmacIniFile = '{}/default_{}_metric.init'.format(self.copyPath, display)
            if line.startswith('[') or not line:
                if result == 1:
                    break
                else:
                    inFile.close()
                    self.dialog_ok('ERROR','Could not find LINEAR_UNITS in [TRAJ] section of INI file')
                    self.W.destroy()
                    return False
        inFile.close()
        return True

    def get_kins_info(self,readFile):
        # get some info from [KINS] section of INI file copy
        inFile = open(readFile,'r')
        while 1:
            line = inFile.readline()
            if '[KINS]' in line:
                break
            if not line:
                inFile.close()
                self.dialog_ok('ERROR','Cannot find [TRAJ] section in INI file')
                self.W.destroy()
                return False
        kinsLine = jntsLine = ''
        while 1:
            line = inFile.readline()
            if line.startswith('KINEMATICS'):
                kinsLine = line
            elif line.startswith('JOINTS'):
                jntsLine = line
            elif line.startswith('[') or not line:
                if kinsLine and jntsLine:
                    numJoints = int(jntsLine.strip().replace(' ','').split('=')[1])
                    if 'coordinates' in kinsLine:
                        a,b = kinsLine.lower().strip().replace(' ','').split('coordinates=')
                        if 'kinstype' in b:
                            b = b.split('kinstype')[0]
                    else:
                        b = 'xyzabcuvw'[:numJoints]
                    self.zJoint = b.index('z')
                    break
                else:
                    inFile.close()
                    if not kinsLine:
                        self.dialog_ok('ERROR','Could not find KINEMATICS in [KINS] section of INI file')
                    if not jntsLine:
                        self.dialog_ok('ERROR','Could not find JOINTS in [KINS] section of INI file')
                    self.W.destroy()
                    return False
        inFile.close()
        return True

    def write_new_hal_file(self):
        #write new hal file
        newHalFile = open('{}/{}{}'.format(self.configDir,self.machineName.lower(),self.halExt),'w')
        inHal = open(self.readHalFile,'r')
        for line in inHal:
            # add spindles to motmod
            if line.replace(' ','').startswith('loadrtmotmod') or line.replace(' ','').startswith('loadrt[EMCMOT]EMCMOT'):
                if 'num_spindles' in line:
                    line = line.split('num_spindles')[0].strip()
                line = '{} num_spindles=[TRAJ]SPINDLES\n'.format(line.strip())
            # reformat lines from 7i96 config tool to run with twopass
            elif line.replace(' ','').startswith('loadrt[HOSTMOT2]DRIVER') or \
                 line.replace(' ','').startswith('loadrt[HOSTMOT2](DRIVER)'):
                if line.replace(' ','').startswith('loadrt[HOSTMOT2](DRIVER)'):
                 for r in (('(',''), (')','')):
                     line = line.replace(*r)
                hostmot = {}
                inFile = open('{}'.format(self.orgIniFile), 'r')
                while 1:
                    inf = inFile.readline()
                    if inf.startswith('[HOSTMOT2]'):
                        break
                    if not inf:
                        inFile.close()
                        self.dialog_ok('ERROR','Cannot find [HOSTMOT2] section in INI file')
                        return None
                while 1:
                    inf = inFile.readline()
                    if inf.startswith('[') or not inf:
                        break
                    elif '=' in inf:
                        a,b = inf.strip().split('=')
                        hostmot[a.strip()] = b.strip()
                line = line.replace('[HOSTMOT2]DRIVER',hostmot['DRIVER'])
                for param in ['IPADDRESS','ENCODERS','STEPGENS','PWMGENS','SSERIAL_PORT']:
                    if param in hostmot:
                        line = line.replace('[HOSTMOT2]' + param,hostmot[param])
            # comment out old spindle and halui lines
            elif 'spindle' in line.lower() or 'halui.machine.is-on' in line.lower():
                line = '# {}'.format(line)
            # comment out old toolchange lines
            elif 'hal_manualtoolchange' in line or 'iocontrol.0.tool' in line:
                line = '# {}'.format(line)
            newHalFile.write(line)
        newHalFile.write(\
            '\n# toolchange passthrough\n'
            'net tool:change iocontrol.0.tool-change  => iocontrol.0.tool-changed\n'
            'net tool:prep   iocontrol.0.tool-prepare => iocontrol.0.tool-prepared\n')
        newHalFile.close()
        inHal.close()
        return True

    def write_connections_hal_file(self):
        # write a connections.hal file for plasmac connections to the machine
        with open('{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()), 'w') as outFile:
            outFile.write(\
                '# Keep your plasmac i/o connections here to prevent them from\n'\
                '# being overwritten by updates or pncconf/stepconf changes\n\n'\
                '# Other customisations may be placed here as well\n'\
                '# This file is built by the configurator in your configuration directory\n\n'\
                '#***** DEBOUNCE FOR THE INPUTS *****\n'\
                'loadrt dbounce names=db_breakaway,db_float,db_ohmic,db_arc-ok\n'\
                'addf db_float     servo-thread\n'\
                'addf db_ohmic     servo-thread\n'\
                'addf db_breakaway servo-thread\n'\
                'addf db_arc-ok    servo-thread\n'\
                '# for the float and ohmic inputs\n'\
                '# each increment in delay is a 0.001mm (0.00004") increase in any probed height result\n'\
                'setp db_float.delay     5\n'\
                'setp db_ohmic.delay     5\n'\
                'setp db_breakaway.delay 5\n'\
                'setp db_arc-ok.delay    5\n\n'\
                '#***** ARC VOLTAGE LOWPASS FILTER *****\n'\
                '# set the cutoff frequency if required\n'\
                'setp plasmac.lowpass-frequency 0\n\n'\
                '#***** THE JOINT ASSOCIATED WITH THE Z AXIS *****\n')
            outFile.write('net plasmac:axis-position joint.{:d}.pos-fb => plasmac.axis-z-position\n\n'.format(self.zJoint))
            outFile.write('#***** PLASMA CONNECTIONS *****\n')
            if self.arcVoltPin.get_text() and (self.mode == 0 or self.mode == 1):
                outFile.write('net plasmac:arc-voltage-in {} => plasmac.arc-voltage-in\n'.format(self.arcVoltPin.get_text()))
            if self.arcOkPin.get_text() and (self.mode == 1 or self.mode == 2):
                outFile.write('net plasmac:arc-ok-in {} => db_arc-ok.in\n'.format(self.arcOkPin.get_text()))
            if self.floatPin.get_text():
                outFile.write('net plasmac:float-switch {} => db_float.in\n'.format(self.floatPin.get_text()))
            elif not self.floatPin.get_text():
                outFile.write('# net plasmac:float-switch {YOUR FLOAT SWITCH PIN} => db_float.in\n')
            if self.breakPin.get_text():
                outFile.write('net plasmac:breakaway {} => db_breakaway.in\n'.format(self.breakPin.get_text()))
            elif not self.breakPin.get_text():
                outFile.write('# net plasmac:breakaway {YOUR BREAKAWAY PIN} => db_breakaway.in\n')
            if self.ohmicInPin.get_text():
                outFile.write('net plasmac:ohmic-probe {} => db_ohmic.in\n'.format(self.ohmicInPin.get_text()))
            elif not self.ohmicInPin.get_text():
                outFile.write('# net plasmac:ohmic-probe {YOUR OHMIC PROBE PIN} => db_ohmic.in\n')
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
            outFile.write('\n#***** MULTIPLE TOOL ENABLE *****\n' \
                            '# set to 1 to enable a scribe or spotting\n' \
                            'setp plasmac.multi-tool 0\n\n')
            outFile.write('#***** SCRIBE CONNECTIONS *****\n')
            if self.scribeArmPin.get_text():
                outFile.write('net plasmac:scribe-arm plasmac.scribe-arm => {}\n'.format(self.scribeArmPin.get_text()))
            else:
                outFile.write('# net plasmac:scribe-arm plasmac.scribe-arm => ***YOUR_SCRIBE_ARMING_OUTPUT***\n')
            if self.scribeOnPin.get_text():
                outFile.write('net plasmac:scribe-on  plasmac.scribe-on  => {}\n'.format(self.scribeOnPin.get_text()))
            else:
                outFile.write('# net plasmac:scribe-on  plasmac.scribe-on  => ***YOUR_SCRIBE_ON_OUTPUT***\n')
        return True

    def write_postgui_hal_file(self):
        # create a postgui.tcl HAL file if not already present
        if not os.path.exists('{}/postgui.tcl'.format(self.configDir)):
            with open('{}/postgui.tcl'.format(self.configDir), 'w') as outFile:
                outFile.write(\
                    '# Keep your post GUI customisations here to prevent them from being overwritten\n'\
                    '# by updates or pncconf/stepconf changes.\n\n'\
                    '# As an example:\n'\
                    '# You currently have a plasmac:thc-enable signal which connects the\n'\
                    '# plasmac_run.thc-enable-out output to the plasmac.thc-enable input.\n\n'\
                    '# You want to connect the thc-enable pin of the plasmac component to a switch\n'\
                    '# on your machine rather than let it be controlled from the GUI Run tab.\n\n'\
                    '# First disconnect the GUI Run tab from the plasmac:thc-enable signal:\n'\
                    '# unlinkp plasmac_run.thc.enable-out\n\n'\
                    '# Then connect the plasmac:thc-enable signal to your switch:\n'\
                    '# net plasmac:thc-enable your.switch-pin\n'\
                    )
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
                elif line.startswith('TORCH_PULSE_TIME'):
                    outFile.write(line)
                elif line.startswith('#PM_PORT') and self.pmPortName.get_text():
                    outFile.write('PM_PORT                 = {}\n'.format(self.pmPortName.get_text()))
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
            'HALFILE = ./plasmac/plasmac.tcl\n'\
            '# the plasma machine  and custom connections\n'\
            'HALFILE = {0}_connections.hal\n'\
            '# use this for customisation after GUI has loaded\n'\
            'POSTGUI_HALFILE = postgui.tcl\n'\
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
        toolFile = False
        cycleTime = False
        while 1:
            line = inFile.readline()
            if not line.startswith('['):
                if line.startswith('OPEN_FILE'):
                    outFile.write('OPEN_FILE = \"\"\n')
                    openFile = True
                elif line.startswith('TOOL_EDITOR'):
                    outFile.write('TOOL_EDITOR = tooledit x y\n')
                    toolFile = True
                elif line.startswith('CYCLE_TIME'):
                    outFile.write('CYCLE_TIME = 0.1\n')
                    cycleTime = True
                elif line.startswith('PYVCP') or line.startswith('GLADEVCP'):
                    pass
                elif line.strip():
                    outFile.write(line)
            else:
                inFile.close()
                break
        if not openFile and display == 'axis':
            outFile.write('OPEN_FILE = \"\"\n')
        if not toolFile and display == 'axis':
            outFile.write('TOOL_EDITOR = tooledit x y\n')
        if not cycleTime and display == 'axis':
            outFile.write('CYCLE_TIME = 0.1\n')
        outFile.write('\n')
        if display == 'axis':
            outFile.write(\
                '# required\n'\
                'USER_COMMAND_FILE = ./plasmac/plasmac_axis.py\n\n')
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
                    if '= Run' in line:
                        outFile.write('#{}'.format(line))
                    elif 'plasmac_run_tab.glade' in line:
                        outFile.write('#{}'.format(line))
                    elif 'plasmac_run_panel.glade' in line:
                        outFile.write('GLADEVCP                = -c plasmac_run -u ./plasmac/plasmac_run.py -H ./plasmac/plasmac_run.hal ./plasmac/plasmac_run_panel.glade\n')
                    else:
                        outFile.write(line)
                elif display == 'gmoccapy' and self.panel:
                    if '= Run' in line:
                        plasmaRun = True
                    if 'ntb_preview' in line and plasmaRun:
                        outFile.write('#{}'.format(line))
                        plasmaRun = False
                    elif 'plasmac_run_tab.glade' in line:
                        outFile.write('#{}'.format(line))
                    elif 'box_left' in line:
                        outFile.write('EMBED_TAB_LOCATION      = box_left\n')
                    elif 'plasmac_run_panel.glade' in line:
                        outFile.write('EMBED_TAB_COMMAND       = gladevcp -c plasmac_run -x {XID} -u ./plasmac/plasmac_run.py -H ./plasmac/plasmac_run.hal ./plasmac/plasmac_run_panel.glade\n')
                    else:
                        outFile.write(line)
                else:
                    outFile.write(line)
            else:
                break
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
        editSection = False
        offsetAxis = False
        newName = False
        addSpindle = False
        while 1:
            line = inFile.readline()
            if line.startswith('['):
                newName = False
                offsetAxis = False
                addSpindle = False
                if line.strip() in done:
                    editSection = False
                else:
                    done.append(line.strip())
                    editSection = True
                if line.strip() == '[EMC]' and editSection:
                    newName = True
                elif line.strip() == '[TRAJ]' and editSection:
                    addSpindle = True
                elif (line.strip() == '[AXIS_X]' or line.strip() == '[AXIS_Y]' or line.strip() == '[AXIS_Z]') and editSection:
                    offsetAxis = True
            if editSection:
                if newName:
                    if line.startswith('MACHINE'):
                        outFile.write('MACHINE = {}\n'.format(self.machineName))
                    else:
                        outFile.write(line)
                elif addSpindle:
                    if line.strip() == '[TRAJ]':
                        outFile.write(line)
                        outFile.write('SPINDLES = 3\n')
                    else:
                        outFile.write(line)
                elif offsetAxis:
                    if line.startswith('MAX_VELOCITY'):
                        a,b = line.strip().replace(' ','').split('=')
                        outFile.write(
                            '# set to double the value in the corresponding joint\n'\
                            'MAX_VELOCITY = {}\n'.format(float(b) * 2))
                    elif line.startswith('MAX_ACCELERATION'):
                        a,b = line.strip().replace(' ','').split('=')
                        outFile.write(\
                            '# set to double the value in the corresponding joint\n'\
                            'MAX_ACCELERATION = {}\n'\
                            '# shares the above two equally between the joint and the offset\n'\
                            'OFFSET_AV_RATIO = 0.5\n'.format(float(b) * 2))
                    else:
                        outFile.write(line)
                else:
                    outFile.write(line)
            if not line:
                break
        outFile.close()
        inFile.close()
        return True

    def make_links(self,display, versionMajor):
        # remove plasmac.hal from versions 0.087 and older
        if self.configureType == 'upgrade' and versionMajor <= 0.087:
            fNname = '{}/plasmac.hal'.format(self.configDir)
            if os.path.islink(fNname):
                os.unlink(fNname)
            elif os.path.isdir(fNname):
                shutil.rmtree(fNname, ignore_errors=True)
            elif os.path.exists(fNname):
                os.remove(fNname)
        # remove existing links to the plasmac source from config directory
        for file in oldFileList:
            fName = os.path.join(self.configDir, file)
            if os.path.islink(fName):
                os.unlink(fName)
            elif os.path.isfile(fName):
                os.remove(fName)
            elif os.path.isdir(fName):
                shutil.rmtree(fName, ignore_errors=True)
        # if plasmac directory exists remove all existing links
        plasDir = '{}/plasmac'.format(self.configDir)
        if os.path.exists(plasDir):
            if os.path.islink(plasDir):
                os.unlink(plasDir)
                os.mkdir(plasDir)
            else:
                for fileName in os.listdir(plasDir):
                    if os.path.islink('{}/{}'.format(plasDir, fileName)):
                        os.unlink('{}/{}'.format(plasDir, fileName))
                    elif os.path.isfile('{}/{}'.format(plasDir, fileName)):
                        os.remove('{}/{}'.format(plasDir, fileName))
        else:
            os.mkdir('{}/plasmac'.format(self.configDir))
        # new links in config directory
        for fileName in ['configurator.py','materialverter.py','pmx_test.py','versions.html']:
            src = '{}/{}'.format(self.copyPath,fileName)
            dst = '{}/{}'.format(self.configDir,fileName)
            os.symlink(src,dst)
        # new links in plasmac directory
        for fileName in newFileList:
            src = '{}/{}'.format(self.copyPath,fileName)
            if fileName == 'wizards' or fileName == 'test':
                dst = '{}/{}'.format(self.configDir,fileName)
            else:
                dst = '{}/plasmac/{}'.format(self.configDir,fileName)
            os.symlink(src,dst)
        # the tool table is special, it needs to be a file
        # and if it is don't overwrite it
        for fileName in ['tool.tbl']:
            src = '{}/{}'.format(self.copyPath,fileName)
            dst = '{}/{}'.format(self.configDir,fileName)
            if os.path.islink(dst):
                os.unlink(dst)
                shutil.copy(src,dst)
            elif self.configureType == 'new':
                shutil.copy(src,dst)
        return True

    def write_material_file(self):
        # create a new material file if not existing
        materialFile = '{}/{}_material.cfg'.format(self.configDir,self.machineName.lower())
        if os.path.exists(materialFile):
            return True
        else: # create a new material file if it doesn't exist
            with open(materialFile, 'w') as outFile:
                outFile.write(self.material_header())
        return True

    def reconfigure(self):
        if self.mode != self.oldMode or self.panel != self.oldPanel or self.oldPmPortName != self.pmPortName.get_text():
            shutil.copy(self.orgIniFile,self.orgIniFile + '.bakr')
            outFile = open('{}'.format(self.orgIniFile), 'w')
            with open('{}.bakr'.format(self.orgIniFile), 'r') as inFile:
                while 1:
                    line = inFile.readline()
                    if not line:
                        break
                    elif line.startswith('MODE') and self.mode != self.oldMode:
                        self.oldMode = self.mode
                        outFile.write('MODE = {}\n'.format(self.mode))
                    elif line.startswith('PM_PORT') and self.oldPmPortName != self.pmPortName.get_text():
                        self.oldPmPortName = self.pmPortName.get_text()
                        outFile.write('PM_PORT = {}\n'.format(self.pmPortName.get_text()))
                    elif self.panel != self.oldPanel and 'EMBED_TAB_NAME' in line and 'Plasma Run' in line and self.display == 'axis':
                        if line.startswith('#'):
                            outFile.write(line.lstrip('#'))
                        else:
                            outFile.write('#{}'.format(line))
                    elif self.panel != self.oldPanel and 'EMBED_TAB_COMMAND' in line and 'plasmac_run' in line and self.display == 'axis':
                        if line.startswith('#'):
                            outFile.write(line.lstrip('#'))
                        else:
                            outFile.write('#{}'.format(line))
                    elif self.panel != self.oldPanel and 'GLADEVCP' in line and 'plasmac_run' in line and self.display == 'axis':
                        if line.startswith('#'):
                            outFile.write(line.lstrip('#'))
                        else:
                            outFile.write('#{}'.format(line))
                    elif self.panel != self.oldPanel and 'EMBED_TAB_LOCATION' in line and ('ntb_preview' in line or 'box_left' in line) and self.display == 'gmoccapy':
                        if line.startswith('#'):
                            outFile.write(line.lstrip('#'))
                        else:
                            outFile.write('#{}'.format(line))
                    elif self.panel != self.oldPanel and 'EMBED_TAB_COMMAND' in line and 'plasmac_run' in line and self.display == 'gmoccapy':
                        if line.startswith('#'):
                            outFile.write(line.lstrip('#'))
                        else:
                            outFile.write('#{}'.format(line))
                    else:
                        outFile.write(line)
        arcVoltMissing = True
        arcOkMissing = True
        moveUpMissing = True
        moveDownMissing = True
        newConex = '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower())
        oldConex = '{}.bakr'.format(newConex)
        shutil.copy(newConex, oldConex)
        outFile = open(newConex, 'w')
        with open(oldConex, 'r') as inFile:
            for line in inFile:
                newLine = line
                if 'arc-voltage-in' in line:
                    arcVoltMissing = False
                    if self.arcVoltPin.get_text() and (self.mode == 0 or self.mode == 1):
                        if self.oldArcVoltPin != self.arcVoltPin.get_text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldArcVoltPin)
                            outFile.write('{}{}{}\n'.format(a, self.arcVoltPin.get_text(), b))
                            self.oldArcVoltPin = self.arcVoltPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'plasmac:arc-ok-in' in line:
                    arcOkMissing = False
                    if self.arcOkPin.get_text() and (self.mode == 1 or self.mode == 2):
                        if self.oldArcOkPin != self.arcOkPin.get_text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldArcOkPin)
                            outFile.write('{}{}{}\n'.format(a, self.arcOkPin.get_text(), b))
                            self.oldArcOkPin = self.arcOkPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'ohmic-probe' in line:
                    if self.ohmicInPin.get_text():
                        if self.oldOhmicInPin != self.ohmicInPin.get_text():
                            a, b = line.strip('#').strip().split(self.oldOhmicInPin)
                            outFile.write('{}{}{}\n'.format(a, self.ohmicInPin.get_text(), b))
                            self.oldOhmicInPin = self.ohmicInPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'ohmic-enable' in line:
                    if self.ohmicOutPin.get_text():
                        if self.oldOhmicOutPin != self.ohmicOutPin.get_text():
                            a, b = line.strip('#').strip().split(self.oldOhmicOutPin)
                            outFile.write('{}{}{}\n'.format(a, self.ohmicOutPin.get_text(), b))
                            self.oldOhmicOutPin = self.ohmicOutPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'float-switch' in line:
                    if self.floatPin.get_text():
                        if self.oldFloatPin != self.floatPin.get_text():
                            a, b = line.strip('#').strip().split(self.oldFloatPin)
                            outFile.write('{}{}{}\n'.format(a, self.floatPin.get_text(), b))
                            self.oldFloatPin = self.floatPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'breakaway' in line:
                    if self.breakPin.get_text():
                        if self.oldBreakPin != self.breakPin.get_text():
                            a, b = line.strip('#').strip().split(self.oldBreakPin)
                            outFile.write('{}{}{}\n'.format(a, self.breakPin.get_text(), b))
                            self.oldBreakPin = self.breakPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'torch-on' in line:
                    if self.torchPin.get_text():
                        if self.oldTorchPin != self.torchPin.get_text():
                            a = line.strip('#').rsplit(self.oldTorchPin, 1)[0]
                            outFile.write('{}{}\n'.format(a, self.torchPin.get_text()))
                            self.oldTorchPin = self.torchPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'move-up' in line:
                    moveUpMissing = False
                    if self.moveUpPin.get_text() and self.mode == 2:
                        if self.oldMoveUpPin != self.moveUpPin.get_text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldMoveUpPin)
                            outFile.write('{}{}{}\n'.format(a, self.moveUpPin.get_text(), b))
                            self.oldMoveUpPin = self.moveUpPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'move-down' in line:
                    moveDownMissing = False
                    if self.moveDownPin.get_text() and self.mode == 2:
                        if self.oldMoveDownPin != self.moveDownPin.get_text() or self.oldMode != self.mode:
                            a, b = line.strip('#').strip().split(self.oldMoveDownPin)
                            outFile.write('{}{}{}\n'.format(a, self.moveDownPin.get_text(), b))
                            self.oldMoveDownPin = self.moveDownPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'scribe-arm' in line:
                    if self.scribeArmPin.get_text():
                        if self.oldScribeArmPin != self.scribeArmPin.get_text():
                            a, b = line.strip('#').strip().split(self.oldScribeArmPin)
                            outFile.write('{}{}{}\n'.format(a, self.scribeArmPin.get_text(), b))
                            self.oldScribeArmPin = self.scribeArmPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                elif 'scribe-on' in line:
                    if self.scribeOnPin.get_text():
                        if self.oldscribeOnPin != self.scribeOnPin.get_text():
                            a, b = line.strip('#').strip().split(self.oldscribeOnPin)
                            outFile.write('{}{}{}\n'.format(a, self.scribeOnPin.get_text(), b))
                            self.oldscribeOnPin = self.scribeOnPin.get_text()
                        else:
                            outFile.write('{}\n'.format(line.strip('#').strip()))
                    elif line.startswith('#'):
                        outFile.write(line)
                    else:
                        outFile.write('# {}'.format(line))
                else:
                    outFile.write(line)
        if arcVoltMissing and self.arcVoltPin.get_text():
            outFile.write('net plasmac:arc-voltage-in {} => plasmac.arc-voltage-in\n'.format(self.arcVoltPin.get_text()))
        if arcOkMissing and self.arcOkPin.get_text():
            outFile.write('net plasmac:arc-ok-in {} => plasmac.arc-ok-in\n'.format(self.arcOkPin.get_text()))
        if moveUpMissing and self.moveUpPin.get_text():
            outFile.write('net plasmac:move-up {} => plasmac.move-up\n'.format(self.moveUpPin.get_text()))
        if moveDownMissing and self.moveDownPin.get_text():
            outFile.write('net plasmac:move-down {} => plasmac.move-down\n'.format(self.moveDownPin.get_text()))
        self.oldMode = self.mode
        return

    def populate_reconfigure(self):
        if not self.modeSet:
            with open(self.orgIniFile,'r') as inFile:
                while 1:
                    line = inFile.readline()
                    if line.startswith('[PLASMAC]'): break
                while 1:
                    line = inFile.readline()
                    if line.startswith('MODE'):
                        self.oldMode = int(line.split('=')[1].strip())
                        inFile.close()
                        break
                    elif line.startswith('[') or not line:
                        inFile.close()
                        break
            [self.mode0, self.mode1, self.mode2][self.oldMode].set_active(True)
            self.modeSet = True
        self.arcVoltPin.set_text('')
        self.oldArcVoltPin = ''
        self.arcOkPin.set_text('')
        self.oldArcOkPin = ''
        self.ohmicInPin.set_text('')
        self.oldOhmicInPin = ''
        self.ohmicOutPin.set_text('')
        self.oldOhmicOutPin = ''
        self.floatPin.set_text('')
        self.oldFloatPin = ''
        self.breakPin.set_text('')
        self.oldBreakPin = ''
        self.torchPin.set_text('')
        self.oldTorchPin = ''
        self.moveUpPin.set_text('')
        self.oldMoveUpPin = ''
        self.moveDownPin.set_text('')
        self.oldMoveDownPin = ''
        self.scribeArmPin.set_text('')
        self.oldScribeArmPin = ''
        self.scribeOnPin.set_text('')
        self.oldScribeOnPin = ''
        self.pmPortName.set_text('')
        self.oldPmPortName = ''
        try:
            with open('{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()), 'r') as inFile:
                for line in inFile:
                    if 'arc-voltage-in' in line:
                        self.oldArcVoltPin = (line.split('age-in', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.arcVoltPin.set_text(self.oldArcVoltPin)
                    elif 'plasmac:arc-ok-in' in line:
                        self.oldArcOkPin = (line.split('plasmac:arc-ok-in', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.arcOkPin.set_text(self.oldArcOkPin)
                    elif 'ohmic-probe' in line:
                        self.oldOhmicInPin = (line.split('-probe', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.ohmicInPin.set_text(self.oldOhmicInPin)
                    elif 'ohmic-enable' in line:
                        self.oldOhmicOutPin = (line.strip().split(' ' )[-1].strip())
                        if not line.strip().startswith('#'):
                            self.ohmicOutPin.set_text(self.oldOhmicOutPin)
                    elif 'float-switch' in line:
                        self.oldFloatPin = (line.split('-switch', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.floatPin.set_text(self.oldFloatPin)
                    elif 'breakaway' in line:
                        self.oldBreakPin = (line.split('breakaway', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.breakPin.set_text(self.oldBreakPin)
                    elif 'torch-on' in line:
                        self.oldTorchPin = (line.strip().split(' ')[-1].strip())
                        if not line.strip().startswith('#'):
                            self.torchPin.set_text(self.oldTorchPin)
                    elif 'move-up' in line:
                        self.oldMoveUpPin = (line.split('move-up', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.moveUpPin.set_text(self.oldMoveUpPin)
                    elif 'move-down' in line:
                        self.oldMoveDownPin = (line.split('move-down', 1)[1].strip().split(' ', 1)[0].strip())
                        if not line.strip().startswith('#'):
                            self.moveDownPin.set_text(self.oldMoveDownPin)
                    elif 'scribe-arm' in line:
                        self.oldScribeArmPin = (line.strip().split(' ')[-1].strip())
                        if not line.strip().startswith('#'):
                            self.scribeArmPin.set_text(self.oldScribeArmPin)
                    elif 'scribe-on' in line:
                        self.oldscribeOnPin = (line.strip().split(' ')[-1].strip())
                        if not line.strip().startswith('#'):
                            self.scribeOnPin.set_text(self.oldscribeOnPin)
        except:
            self.iniFile.set_text('')
            self.dialog_ok(
                'FILE ERROR',
                '\nCannot open connections file:\n'
                '{}/{}_connections.hal'.format(self.configDir,self.machineName.lower()))
#            return
        with open(self.orgIniFile,'r') as inFile:
            while 1:
                line = inFile.readline()
                if line.startswith('[PLASMAC]'):
                    count = 0
                    break
            while 1:
                line = inFile.readline()
                if line.startswith('[DISPLAY]') or not line: break
                elif line.startswith('PM_PORT'):
                    self.oldPmPortName = line.split('=')[1].strip()
                    self.pmPortName.set_text(self.oldPmPortName)
                    break
            while 1:
                line = inFile.readline()
                if not line: break
                elif line.startswith('DISPLAY'):
                    if'axis' in line.lower():
                        self.display = 'axis'
                    elif 'gmoccapy' in line.lower():
                        self.display = 'gmoccapy'
                elif 'plasmac_run_tab.glade' in line and not line.strip().startswith('#'):
                    self.tabPanel0.set_active(True)
                    self.oldPanel = 0
                    break
                elif 'plasmac_run_panel.glade' in line and not line.strip().startswith('#'):
                    self.tabPanel1.set_active(True)
                    self.oldPanel = 1
                    break
        self.set_mode()

    def success_dialog(self):
        if '/usr/share/doc' in self.gitPath:
            cmd = 'linuxcnc'
        else:
            cmd = '{}/scripts/linuxcnc'.format(self.gitPath)
        self.dialog_ok(\
            'INSTALLATION',\
            '\nConfiguration is complete.\n\n'\
            'You can run this configuration from a console as follows:\n\n'\
            ' {} {}/{}.ini \n\n'\
            'Then adjust required settings on the Config tab\n\n'\
            .format(cmd,self.configDir,self.machineName.lower()))

    def create_widgets(self):
        hb = gtk.HBox(spacing = 20)
        vBL = gtk.VBox()
        vBR = gtk.VBox()
        headerBoxL = gtk.VBox()

        if self.configureType == 'upgrade':
            headerL = gtk.Label('')
        else:
            headerL = gtk.Label('Mandatory Settings')
        headerL.set_alignment(0,0)
        headerBoxL.pack_start(headerL)
        headerLBlank = gtk.Label('')
        headerBoxL.pack_start(headerLBlank)
        vBL.pack_start(headerBoxL, expand=False)
        headerBoxR = gtk.VBox()
        if self.configureType == 'upgrade':
            headerR = gtk.Label('')
        else:
            headerR = gtk.Label('Optional Settings')
        headerR.set_alignment(0,0)
        headerBoxR.pack_start(headerR)
        headerRBlank = gtk.Label('')
        headerBoxR.pack_start(headerRBlank)
        vBR.pack_start(headerBoxR, expand=False)
        if self.configureType == 'new':
            self.nameVBox = gtk.VBox()
            nameLabel = gtk.Label('Machine Name:')
            nameLabel.set_alignment(0,0)
            self.nameFile = gtk.Entry()
            self.nameFile.set_width_chars(40)
            self.nameFile.set_tooltip_markup(\
                'The <b>name</b> of the new or existing machine.\n'\
                'If not existing, this creates a directory ~/linuxcnc/configs/<b>name</b>.\n'\
                '<b>name.ini</b> and <b>name.hal</b> are then written to this directory '\
                'as well as other required files and links to appplication files.\n\n')
            nameBlank = gtk.Label('')
            self.nameVBox.pack_start(nameLabel)
            self.nameVBox.pack_start(self.nameFile)
            self.nameVBox.pack_start(nameBlank)
            vBL.pack_start(self.nameVBox, expand=False)
        self.iniVBox = gtk.VBox()
        if self.configureType == 'new':
            self.iniLabel = gtk.Label('INI file in existing working config:')
        elif self.configureType == 'upgrade':
            self.iniLabel = gtk.Label('INI file of configuration to upgrade:')
        else:
            self.iniLabel = gtk.Label('INI file of configuration to modify:')
        self.iniLabel.set_alignment(0,0)
        self.iniFile = gtk.Entry()
        self.iniFile.set_width_chars(40)
        self.iniBlank = gtk.Label('')
        self.iniVBox.pack_start(self.iniLabel)
        self.iniVBox.pack_start(self.iniFile)
        self.iniVBox.pack_start(self.iniBlank)
        vBL.pack_start(self.iniVBox,expand=False)
        if self.configureType == 'new':
            self.halVBox = gtk.VBox()
            halLabel = gtk.Label('HAL file in existing working config:')
            halLabel.set_alignment(0,0)
            self.halFile = gtk.Entry()
            self.halFile.set_width_chars(40)
            halBlank = gtk.Label('')
            self.halVBox.pack_start(halLabel)
            self.halVBox.pack_start(self.halFile)
            self.halVBox.pack_start(halBlank)
            vBL.pack_start(self.halVBox,expand=False)
        if self.configureType == 'new' or self.configureType == 'reconfigure':
            self.modeVBox = gtk.VBox()
            self.modeLabel = gtk.Label('Use arc voltage for both Arc-OK and THC')
            self.modeLabel.set_alignment(0,0)
            self.modeHBox = gtk.HBox(homogeneous=True)
            self.mode0 = gtk.RadioButton(group=None, label='Mode: 0')
            self.modeHBox.pack_start(self.mode0)
            self.mode1 = gtk.RadioButton(group=self.mode0, label='Mode: 1')
            self.modeHBox.pack_start(self.mode1)
            self.mode2 = gtk.RadioButton(group=self.mode0, label='Mode: 2')
            self.modeHBox.pack_start(self.mode2)
            modeBlank = gtk.Label('')
            self.modeVBox.pack_start(self.modeLabel)
            self.modeVBox.pack_start(self.modeHBox)
            self.modeVBox.pack_start(modeBlank)
            vBL.pack_start(self.modeVBox,expand=False)
        if self.configureType == 'new' or self.configureType == 'reconfigure':
            self.arcVoltVBox = gtk.VBox()
            self.arcVoltLabel = gtk.Label('Arc Voltage HAL pin: (float in)')
            self.arcVoltLabel.set_alignment(0,0)
            self.arcVoltPin = gtk.Entry()
            self.arcVoltPin.set_width_chars(40)
            arcVoltBlank = gtk.Label('')
            self.arcVoltVBox.pack_start(self.arcVoltLabel)
            self.arcVoltVBox.pack_start(self.arcVoltPin)
            self.arcVoltVBox.pack_start(arcVoltBlank)
            vBL.pack_start(self.arcVoltVBox,expand=False)
            self.arcOkVBox = gtk.VBox()
            self.arcOkLabel = gtk.Label('Arc OK HAL pin: (bit in)')
            self.arcOkLabel.set_alignment(0,0)
            self.arcOkPin = gtk.Entry()
            self.arcOkPin.set_width_chars(40)
            arcOkBlank = gtk.Label('')
            self.arcOkVBox.pack_start(self.arcOkLabel)
            self.arcOkVBox.pack_start(self.arcOkPin)
            self.arcOkVBox.pack_start(arcOkBlank)
            vBL.pack_start(self.arcOkVBox,expand=False)
            self.ohmicInVBox = gtk.VBox()
            self.torchVBox = gtk.VBox()
            torchLabel = gtk.Label('Torch On HAL pin: (bit out)')
            torchLabel.set_alignment(0,0)
            self.torchPin = gtk.Entry()
            self.torchPin.set_width_chars(40)
            torchBlank = gtk.Label('')
            self.torchVBox.pack_start(torchLabel)
            self.torchVBox.pack_start(self.torchPin)
            self.torchVBox.pack_start(torchBlank)
            vBL.pack_start(self.torchVBox,expand=False)
            self.moveUpVBox = gtk.VBox()
            self.moveUpLabel = gtk.Label('Move Up HAL pin: (bit in)')
            self.moveUpLabel.set_alignment(0,0)
            self.moveUpPin = gtk.Entry()
            self.moveUpPin.set_width_chars(40)
            moveUpBlank = gtk.Label('')
            self.moveUpVBox.pack_start(self.moveUpLabel)
            self.moveUpVBox.pack_start(self.moveUpPin)
            self.moveUpVBox.pack_start(moveUpBlank)
            vBL.pack_start(self.moveUpVBox,expand=False)
            self.moveDownVBox = gtk.VBox()
            self.moveDownLabel = gtk.Label('Move Down HAL pin: (bit in)')
            self.moveDownLabel.set_alignment(0,0)
            self.moveDownPin = gtk.Entry()
            self.moveDownPin.set_width_chars(40)
            moveDownBlank = gtk.Label('')
            self.moveDownVBox.pack_start(self.moveDownLabel)
            self.moveDownVBox.pack_start(self.moveDownPin)
            self.moveDownVBox.pack_start(moveDownBlank)
            vBL.pack_start(self.moveDownVBox,expand=False)
            self.tabPanelVBox = gtk.VBox()
            self.tabPanelLabel = gtk.Label('Run Frame is a tab behind preview')
            self.tabPanelLabel.set_alignment(0,0)
            self.tabPanelHBox = gtk.HBox(homogeneous=True)
            self.tabPanel0 = gtk.RadioButton(group=None, label='Run Tab')
            self.tabPanelHBox.pack_start(self.tabPanel0)
            self.tabPanel1 = gtk.RadioButton(group=self.tabPanel0, label='Run Panel')
            self.tabPanelHBox.pack_start(self.tabPanel1)
            tabPanelBlank = gtk.Label('')
            self.tabPanelVBox.pack_start(self.tabPanelLabel)
            self.tabPanelVBox.pack_start(self.tabPanelHBox)
            self.tabPanelVBox.pack_start(tabPanelBlank)
            vBL.pack_start(self.tabPanelVBox,expand=False)
            self.floatVBox = gtk.VBox()
            floatLabel = gtk.Label('Float Switch HAL pin: (bit in)')
            floatLabel.set_alignment(0,0)
            self.floatPin = gtk.Entry()
            self.floatPin.set_width_chars(40)
            floatBlank = gtk.Label('')
            self.floatVBox.pack_start(floatLabel)
            self.floatVBox.pack_start(self.floatPin)
            self.floatVBox.pack_start(floatBlank)
            vBR.pack_start(self.floatVBox,expand=False)
            self.breakVBox = gtk.VBox()
            breakLabel = gtk.Label('Breakaway Switch HAL pin: (bit in)')
            breakLabel.set_alignment(0,0)
            self.breakPin = gtk.Entry()
            self.breakPin.set_width_chars(40)
            breakBlank = gtk.Label('')
            self.breakVBox.pack_start(breakLabel)
            self.breakVBox.pack_start(self.breakPin)
            self.breakVBox.pack_start(breakBlank)
            vBR.pack_start(self.breakVBox,expand=False)
            ohmicInLabel = gtk.Label('Ohmic Probe HAL pin: (bit in)')
            ohmicInLabel.set_alignment(0,0)
            self.ohmicInPin = gtk.Entry()
            self.ohmicInPin.set_width_chars(40)
            ohmicInBlank = gtk.Label('')
            self.ohmicInVBox.pack_start(ohmicInLabel)
            self.ohmicInVBox.pack_start(self.ohmicInPin)
            self.ohmicInVBox.pack_start(ohmicInBlank)
            vBR.pack_start(self.ohmicInVBox,expand=False)
            self.ohmicOutVBox = gtk.VBox()
            ohmicOutLabel = gtk.Label('Ohmic Probe Enable HAL pin: (bit out)')
            ohmicOutLabel.set_alignment(0,0)
            self.ohmicOutPin = gtk.Entry()
            self.ohmicOutPin.set_width_chars(40)
            ohmicOutBlank = gtk.Label('')
            self.ohmicOutVBox.pack_start(ohmicOutLabel)
            self.ohmicOutVBox.pack_start(self.ohmicOutPin)
            self.ohmicOutVBox.pack_start(ohmicOutBlank)
            vBR.pack_start(self.ohmicOutVBox,expand=False)
            self.scribeArmVBox = gtk.VBox()
            self.scribeArmLabel = gtk.Label('Scribe Arming HAL pin: (bit out)')
            self.scribeArmLabel.set_alignment(0,0)
            self.scribeArmPin = gtk.Entry()
            self.scribeArmPin.set_width_chars(40)
            scribeArmBlank = gtk.Label('')
            self.scribeArmVBox.pack_start(self.scribeArmLabel)
            self.scribeArmVBox.pack_start(self.scribeArmPin)
            self.scribeArmVBox.pack_start(scribeArmBlank)
            vBR.pack_start(self.scribeArmVBox,expand=False)
            self.scribeOnVBox = gtk.VBox()
            self.scribeOnLabel = gtk.Label('Scribe On HAL pin: (bit out)')
            self.scribeOnLabel.set_alignment(0,0)
            self.scribeOnPin = gtk.Entry()
            self.scribeOnPin.set_width_chars(40)
            scribeOnBlank = gtk.Label('')
            self.scribeOnVBox.pack_start(self.scribeOnLabel)
            self.scribeOnVBox.pack_start(self.scribeOnPin)
            self.scribeOnVBox.pack_start(scribeOnBlank)
            vBR.pack_start(self.scribeOnVBox,expand=False)
            self.pmPortVBox = gtk.VBox()
            self.pmPortLabel = gtk.Label('Powermax Com Port: (e.g. /dev/ttyUSB0)')
            self.pmPortLabel.set_alignment(0,0)
            self.pmPortName = gtk.Entry()
            self.pmPortName.set_width_chars(40)
            pmPortBlank = gtk.Label('')
            self.pmPortVBox.pack_start(self.pmPortLabel)
            self.pmPortVBox.pack_start(self.pmPortName)
            self.pmPortVBox.pack_start(pmPortBlank)
            vBR.pack_start(self.pmPortVBox,expand=False)
        BB = gtk.HButtonBox()
        if self.configureType == 'new':
            self.create = gtk.Button('Create')
        elif self.configureType == 'upgrade':
            self.create = gtk.Button('Upgrade')
        else:
            self.create = gtk.Button('Reconfigure')
        self.cancel = gtk.Button('Back')
        BB.pack_start(self.create, True, True, 0)
        BB.pack_start(self.cancel, True, True, 0)
        BB.set_border_width(2)
        vBR.pack_end(BB,expand=False)
        hb.pack_start(vBL)
        hb.pack_start(vBR)
        self.W.add(hb)
        self.W.show_all()
        if self.configureType == 'new':
            self.W.set_title('PlasmaC Config Creator')
            self.modeLabel.set_text('Use arc voltage for both Arc-OK and THC')
            self.tabPanelLabel.set_text('Run Frame is a tab behind preview')
            self.arcVoltVBox.show()
            self.arcOkVBox.hide()
            self.moveUpVBox.hide()
            self.moveDownVBox.hide()
        elif self.configureType == 'upgrade':
            self.W.set_title('PlasmaC Upgrader')
        else:
            self.W.set_title('PlasmaC Reconfigurer')

    def material_header(self):
        return  '# plasmac material file\n'\
                '# example only, may be deleted\n'\
                '# items marked * are mandatory\n'\
                '# other items are optional and will default to 0\n'\
                '#[MATERIAL_NUMBER_1]  \n'\
                '#NAME               = \n'\
                '#KERF_WIDTH         = \n'\
                '#THC                = \n'\
                '#PIERCE_HEIGHT      = *\n'\
                '#PIERCE_DELAY       = *\n'\
                '#PUDDLE_JUMP_HEIGHT = \n'\
                '#PUDDLE_JUMP_DELAY  = \n'\
                '#CUT_HEIGHT         = *\n'\
                '#CUT_SPEED          = *\n'\
                '#CUT_AMPS           = \n'\
                '#CUT_VOLTS          = \n'\
                '#PAUSE_AT_END       = \n'\
                '#GAS_PRESSURE       = \n'\
                '#CUT_MODE           = \n'\
                '\n'

newFileList = [ \
                'imperial_startup.ngc',\
                'M190',\
                'metric_startup.ngc',\
                'plasmac.hal',\
                'plasmac.tcl',\
                'plasmac_axis.py',\
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
                'plasmac_wizards.glade',\
                'plasmac_wizards.py',\
                'pmx485.py',\
                'README',\
                'README.md',\
                'test',\
                'wizards',\
                  ]

oldFileList = [ \
                'blank.ngc',\
                'configurator.py',\
                'imperial_startup.ngc',\
                'M190',\
                'materialverter.py',\
                'metric_startup.ngc',\
                'plasmac.hal',\
                'plasmac.tcl',\
                'plasmac_axis.py',\
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
                'plasmac_wizards.glade',\
                'plasmac_wizards.py',\
                'pmx485.py',\
                'pmx_test.py',\
                'README',\
                'README.md',\
                'versions.html', \
                'test',\
                'wizards',\
                  ]

if __name__ == '__main__':
    try:
        a = configurator()
        gtk.main()
    except KeyboardInterrupt:
        pass
