#!/usr/bin/env python

'''
plasmac_config.py

Copyright (C) 2019, 2020  Phillip A Carter

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
import gtk
import linuxcnc
import gobject
import hal, hal_glib
from   gladevcp.persistence import widget_defaults,select_widgets
import gladevcp
import tarfile

class HandlerClass:

    def on_save_clicked(self,widget,data=None):
        self.save_settings()

    def on_reload_clicked(self,widget,data=None):
        self.load_settings()

    def on_backup_clicked(self,widget):
            outPath = '{}'.format(os.path.expanduser('~'))
            outName = '{}_v{}.tar.gz'.format(self.i.find('EMC', 'MACHINE'), self.plasmacVersion.split('v')[1])
            configDir = os.path.realpath(os.path.dirname(os.environ['INI_FILE_NAME']))
            with tarfile.open('{}/{}'.format(outPath, outName), mode='w:gz', ) as archive:
                archive.add('{}'.format(configDir), filter=self.tar_filter)
            msg  = 'A compressed backup of the machine configuration\n'
            msg += 'has been saved in your home directory.\n\n'
            msg += 'The file name is: {}\n\n'.format(outName)
            msg += 'This file may be attached to a post on the\n'
            msg += 'LinuxCNC forum to aid in problem solving.\n\n'
            md = gtk.MessageDialog(self.W, 
                                   gtk.DIALOG_DESTROY_WITH_PARENT,
                                   gtk.MESSAGE_INFO, 
                                   gtk.BUTTONS_CLOSE,
                                   msg)
            md.set_title('Config Backup')
            md.set_keep_above(True)
            md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
            md.run()
            md.destroy()

    def tar_filter(self, tarinfo):
         if os.path.splitext(tarinfo.name)[1] in ['.pyc']:
              return None
         else:
              return tarinfo

    def on_setupFeedRate_value_changed(self, widget):
        self.builder.get_object('probe-feed-rate-adj').configure(self.builder.get_object('probe-feed-rate').get_value(),0,self.builder.get_object('setup-feed-rate').get_value(),1,0,0)

    def configure_widgets(self):
        # set_digits = number of digits after decimal
        # configure  = (value, lower limit, upper limit, step size, 0, 0)
        self.builder.get_object('version-label').set_text(self.plasmacVersion.strip('PlasmaC '))
        self.builder.get_object('arc-fail-delay').set_digits(1)
        self.builder.get_object('arc-fail-delay-adj').configure(3,0.1,60,0.1,0,0)
        self.builder.get_object('arc-ok-low').set_digits(0)
        self.builder.get_object('arc-ok-low-adj').configure(60,0,100,1,0,0)
        self.builder.get_object('arc-ok-high').set_digits(0)
        self.builder.get_object('arc-ok-high-adj').configure(99999,0,99999,1,0,0)
        self.builder.get_object('arc-max-starts').set_digits(0)
        self.builder.get_object('arc-max-starts-adj').configure(3,1,9,1,0,0)
        self.builder.get_object('arc-restart-delay').set_digits(0)
        self.builder.get_object('arc-restart-delay-adj').configure(3,1,60,1,0,0)
        self.builder.get_object('arc-voltage-offset').set_digits(3)
        self.builder.get_object('arc-voltage-offset-adj').configure(0,-999999,999999,0.001,0,0)
        self.builder.get_object('arc-voltage-scale').set_digits(6)
        self.builder.get_object('arc-voltage-scale-adj').configure(1,-9999,9999,0.000001,0,0)
        self.builder.get_object('spotting-threshold').set_digits(0)
        self.builder.get_object('spotting-threshold-adj').configure(0,0,199,1,0,0)
        self.builder.get_object('spotting-threshold').set_value(0.0)
        self.builder.get_object('spotting-time').set_digits(0)
        self.builder.get_object('spotting-time-adj').configure(0,0,9999,1,0,0)
        self.builder.get_object('spotting-time').set_value(0)
        self.builder.get_object('cornerlock-threshold').set_digits(0)
        self.builder.get_object('cornerlock-threshold-adj').configure(90,1,99,1,0,0)
        self.builder.get_object('kerfcross-override').set_digits(0)
        self.builder.get_object('kerfcross-override-adj').configure(100,10,500,1,0,0)
        self.builder.get_object('max-offset-velocity-in').set_label(str(int(self.thcFeedRate)))
        self.builder.get_object('ohmic-max-attempts').set_digits(0)
        self.builder.get_object('ohmic-max-attempts-adj').configure(0,0,10,1,0,0)
        self.builder.get_object('ohmic-max-attempts').set_value(0)
        self.builder.get_object('pid-p-gain').set_digits(0)
        self.builder.get_object('pid-p-gain-adj').configure(10,0,1000,1,0,0)
        self.builder.get_object('pid-i-gain').set_digits(0)
        self.builder.get_object('pid-i-gain-adj').configure(0,0,1000,1,0,0)
        self.builder.get_object('pid-i-gain').set_value(0)
        self.builder.get_object('pid-d-gain').set_digits(0)
        self.builder.get_object('pid-d-gain-adj').configure(0,0,1000,1,0,0)
        self.builder.get_object('pid-d-gain').set_value(0)
        self.builder.get_object('scribe-arm-delay').set_digits(1)
        self.builder.get_object('scribe-arm-delay-adj').configure(0,0,9,0.1,0,0)
        self.builder.get_object('scribe-on-delay').set_digits(1)
        self.builder.get_object('scribe-on-delay-adj').configure(0,0,9,0.1,0,0)
        self.builder.get_object('thc-delay').set_digits(1)
        self.builder.get_object('thc-delay-adj').configure(1.5,0,9,0.1,0,0)
        self.builder.get_object('thc-threshold').set_digits(2)
        self.builder.get_object('thc-threshold-adj').configure(1,0.05,9,0.01,0,0)
        if self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'mm':
            self.builder.get_object('float-switch-travel').set_digits(2)
            self.builder.get_object('float-switch-travel-adj').configure(1.5,-25,25,0.01,0,0)
            self.builder.get_object('height-per-volt').set_digits(3)
            self.builder.get_object('height-per-volt-adj').configure(0.1,0.025,0.2,0.01,0,0)
            self.builder.get_object('ohmic-probe-offset').set_digits(2)
            self.builder.get_object('ohmic-probe-offset-adj').configure(0,-25,+25,0.01,0,0)
            self.builder.get_object('probe-feed-rate').set_digits(0)
            self.builder.get_object('probe-feed-rate-adj').configure(300,1,self.thcFeedRate,1,0,0)
            self.builder.get_object('probe-start-height').set_digits(0)
            self.builder.get_object('probe-start-height-adj').configure(38,1,self.maxHeight,1,0,0)
            self.builder.get_object('safe-height').set_digits(0)
            self.builder.get_object('safe-height-adj').configure(20,0,self.maxHeight,1,0,0)
            self.builder.get_object('setup-feed-rate').set_digits(0)
            self.builder.get_object('setup-feed-rate-adj').configure(int(self.thcFeedRate * 0.8),1,self.thcFeedRate,1,0,0)
            self.builder.get_object('skip-ihs-distance').set_digits(0)
            self.builder.get_object('skip-ihs-distance-adj').configure(0,0,999,1,0,0)
            self.builder.get_object('skip-ihs-distance').set_value(0)
        elif self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch':
            self.builder.get_object('float-switch-travel').set_digits(3)
            self.builder.get_object('float-switch-travel-adj').configure(0.06,-1,1,0.001,0,0)
            self.builder.get_object('height-per-volt').set_digits(4)
            self.builder.get_object('height-per-volt-adj').configure(0.004,0.001,0.008,0.001,0,0)
            self.builder.get_object('ohmic-probe-offset').set_digits(3)
            self.builder.get_object('ohmic-probe-offset-adj').configure(0,-1,1,0.001,0,0)
            self.builder.get_object('probe-feed-rate').set_digits(1)
            self.builder.get_object('probe-feed-rate-adj').configure(12,0.1,self.thcFeedRate,.1,0,0)
            self.builder.get_object('probe-start-height').set_digits(2)
            self.builder.get_object('probe-start-height-adj').configure(1.5,.1,self.maxHeight,0.01,0,0)
            self.builder.get_object('safe-height').set_digits(2)
            self.builder.get_object('safe-height-adj').configure(0.75,0,self.maxHeight,0.01,0,0)
            self.builder.get_object('setup-feed-rate').set_digits(1)
            self.builder.get_object('setup-feed-rate-adj').configure(int(self.thcFeedRate * 0.8),.1,self.thcFeedRate,.1,0,0)
            self.builder.get_object('skip-ihs-distance').set_digits(1)
            self.builder.get_object('skip-ihs-distance-adj').configure(0,0,99,.1,0,0)
            self.builder.get_object('skip-ihs-distance').set_value(0)
        else:
            print('*** incorrect [TRAJ]LINEAR_UNITS in ini file')

    def periodic(self):
        units = hal.get_value('halui.machine.units-per-mm')
        maxPidP = self.thcFeedRate / units * 0.1
        self.builder.get_object('config').set_sensitive(not hal.get_value('plasmac_config.config-disable'))
        mode = hal.get_value('plasmac.mode')
        units = hal.get_value('halui.machine.units-per-mm')
        maxPidP = self.thcFeedRate / units * 0.1
        if mode != self.oldMode:
            if mode == 0:
                self.builder.get_object('arc-ok-high').show()
                self.builder.get_object('arc-ok-high-label').set_text('OK High Volts')
                self.builder.get_object('arc-ok-low').show()
                self.builder.get_object('arc-ok-low-label').set_text('OK Low Volts')
                self.builder.get_object('arc-voltage-scale').show()
                self.builder.get_object('arc-voltage-scale-label').set_text('Voltage Scale')
                self.builder.get_object('arc-voltage-offset').show()
                self.builder.get_object('arc-voltage-offset-label').set_text('Voltage Offset')
                self.builder.get_object('delay-box').show()
                self.builder.get_object('height-per-volt-box').show()
                self.builder.get_object('kerfcross-override').show()
                self.builder.get_object('kerfcross-override-label').show()
                self.builder.get_object('pid-p-gain-adj').configure(self.builder.get_object('pid-p-gain-adj').get_value(),1,maxPidP,1,0,0)
                self.builder.get_object('pid-p-label').set_text('PID P GAIN (Speed)')
                self.builder.get_object('pid-i-gain').show()
                self.builder.get_object('pid-i-label').set_text('PID I GAIN')
                self.builder.get_object('pid-d-gain').show()
                self.builder.get_object('pid-d-label').set_text('PID D GAIN')
                self.builder.get_object('threshold-box').show()
            elif mode == 1:
                self.builder.get_object('arc-ok-high').hide()
                self.builder.get_object('arc-ok-high-label').set_text('')
                self.builder.get_object('arc-ok-low').hide()
                self.builder.get_object('arc-ok-low-label').set_text('')
                self.builder.get_object('arc-voltage-scale').show()
                self.builder.get_object('arc-voltage-scale-label').set_text('Voltage -scale')
                self.builder.get_object('arc-voltage-offset').show()
                self.builder.get_object('arc-voltage-offset-label').set_text('Voltage -offset')
                self.builder.get_object('delay-box').show()
                self.builder.get_object('height-per-volt-box').show()
                self.builder.get_object('kerfcross-override').show()
                self.builder.get_object('kerfcross-override-label').show()
                self.builder.get_object('pid-p-gain-adj').configure(self.builder.get_object('pid-p-gain-adj').get_value(),1,maxPidP,1,0,0)
                self.builder.get_object('pid-p-label').set_text('PID P GAIN (Speed)')
                self.builder.get_object('pid-i-gain').show()
                self.builder.get_object('pid-i-label').set_text('PID I GAIN')
                self.builder.get_object('pid-d-gain').show()
                self.builder.get_object('pid-d-label').set_text('PID D GAIN')
                self.builder.get_object('threshold-box').show()
            elif mode == 2:
                self.builder.get_object('arc-ok-high').hide()
                self.builder.get_object('arc-ok-high-label').set_text('')
                self.builder.get_object('arc-ok-low').hide()
                self.builder.get_object('arc-ok-low-label').set_text('')
                self.builder.get_object('arc-voltage-scale').hide()
                self.builder.get_object('arc-voltage-scale-label').set_text('')
                self.builder.get_object('arc-voltage-offset').hide()
                self.builder.get_object('arc-voltage-offset-label').set_text('')
                self.builder.get_object('height-per-volt-box').hide()
                self.builder.get_object('delay-box').hide()
                self.builder.get_object('kerfcross-override').hide()
                self.builder.get_object('kerfcross-override-label').hide()
                self.builder.get_object('pid-p-gain-adj').configure(self.builder.get_object('pid-p-gain-adj').get_value(),1,100,1,0,0)
                self.builder.get_object('pid-p-label').set_text('Speed (%)')
                self.builder.get_object('pid-i-gain').hide()
                self.builder.get_object('pid-i-label').set_text('')
                self.builder.get_object('pid-d-gain').hide()
                self.builder.get_object('pid-d-label').set_text('')
                self.builder.get_object('threshold-box').hide()
            else:
                pass
            self.oldMode = mode
        return True

    def set_theme(self):
        theme = gtk.settings_get_default().get_property('gtk-theme-name')
        if os.path.exists(self.prefFile):
            try:
                with open(self.prefFile, 'r') as f_in:
                    for line in f_in:
                        if 'gtk_theme' in line and not 'Follow System Theme' in line:
                            (item, theme) = line.strip().replace(" ", "").split('=')
            except:
                print('*** configuration file, {} is invalid ***'.format(self.prefFile))
        else:
            theme = self.i.find('PLASMAC', 'THEME') or gtk.settings_get_default().get_property('gtk-theme-name')
            font = self.i.find('PLASMAC', 'FONT') or gtk.settings_get_default().get_property('gtk-font-name')
            fSize = int(font.split()[1])
#            font = '{} {}'.format(font.split()[0],fSize - 1 if fSize < 12 else fSize - 2)
            font = '{} {}'.format(font.split()[0],fSize - 1)
            gtk.settings_get_default().set_property('gtk-font-name', font)
        gtk.settings_get_default().set_property('gtk-theme-name', theme)

    def load_settings(self):
        for item in widget_defaults(select_widgets(self.builder.get_objects(), hal_only=False,output_only = True)):
            self.configDict[item] = '0'
        convertFile = False
        if os.path.exists(self.configFile):
            self.upgrade_check()
            try:
                tmpDict = {}
                with open(self.configFile, 'r') as f_in:
                    for line in f_in:
                        if not line.startswith('#') and not line.startswith('[') and not line.startswith('\n'):
                            if line.startswith('version='):
                                pass
                            else:
                                (keyTmp, value) = line.strip().replace(" ", "").split('=')
                                if value == 'True':value = True
                                if value == 'False':value = False
                                key = ''
                                for item in keyTmp:
                                    if item.isupper():
                                        if item == 'C':
                                            key += 'c'
                                        else:
                                            key += '-{}'.format(item.lower())
                                            convertFile = True
                                    else:
                                        key += item
                                if key in self.configDict:
                                    self.configDict[key] = value
                                    tmpDict[key] = value
            except:
                print('*** plasmac config tab configuration file, {} is invalid ***'.format(self.configFile))
            for item in self.configDict:
                if isinstance(self.builder.get_object(item), gladevcp.hal_widgets.HAL_SpinButton):
                    if item in tmpDict:
                        self.builder.get_object(item).set_value(float(self.configDict.get(item)))
                    else:
                        print('*** {} missing from {}'.format(item,self.configFile))
                elif isinstance(self.builder.get_object(item), gladevcp.hal_widgets.HAL_CheckButton):
                    if item in tmpDict:
                        self.builder.get_object(item).set_active(int(self.configDict.get(item)))
                    else:
                        print('*** {} missing from {}'.format(item,self.configFile))
        else:
            self.save_settings()
            print('*** creating new config tab configuration file, {}'.format(self.configFile))

    def save_settings(self):
        try:
            with open(self.configFile, 'w') as f_out:
                f_out.write('# plasmac config tab configuration file\n# format is: name = value\n\n')
                f_out.write('version={}\n\n'.format(self.plasmacVersion))
                for key in sorted(self.configDict.iterkeys()):
                    if isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_SpinButton):
                        self.builder.get_object(key).update()
                        value = self.builder.get_object(key).get_value()
                        f_out.write(key + '=' + str(value) + '\n')
                    elif isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_CheckButton):
                        value = self.builder.get_object(key).get_active()
                        f_out.write(key + '=' + str(value) + '\n')
                    elif key == 'torchPulseTime':
                        value = self.builder.get_object(key).get_value()
                        f_out.write(key + '=' + str(value) + '\n')
        except:
            print('*** error opening {}'.format(self.configFile))

    def upgrade_check(self):
        with open(self.configFile, 'r') as f_in:
            for line in f_in:
                if line.startswith('version='):
                    return
                elif line.startswith('arc-fail-delay'):
                    break
                elif line.startswith('arc-max-starts'):
                    return
        with open(self.configFile, 'r') as f_in:
            contents = f_in.readlines()
        contents[0] = '# plasmac config tab configuration file\n'
        contents[1] = '# format is: name = value\n'
        contents.insert(3, 'version=0.1\n\n')
        runFile = self.i.find('EMC', 'MACHINE').lower() + '_run.cfg'
        if os.path.exists(runFile):
            with open(runFile, 'r') as f_in:
                for line in f_in:
                    if line.startswith('cornerlock-threshold') or \
                       line.startswith('kerfcross-override') or \
                       line.startswith('pid-p-gain') or \
                       line.startswith('thc-delay') or \
                       line.startswith('thc-threshold'):
                        contents.append(line)
            f_out = open(self.configFile, 'w')
            f_out.writelines(contents)
        else:
            print('*** plasmac run tab configuration file, {} is invalid ***'.format(runFile))

    def idle_changed(self, halpin):
        if not halpin.get():
            for key in sorted(self.configDict.iterkeys()):
                if isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_SpinButton):
                    self.builder.get_object(key).update()

    def __init__(self, halcomp,builder,useropts):

        self.plasmacVersion = 'PlasmaC v0.190'

        self.W = gtk.Window()
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        hal_glib.GPin(halcomp.newpin('config-disable', hal.HAL_BIT, hal.HAL_IN))
        configDisable = self.i.find('PLASMAC', 'CONFIG_DISABLE') or '0'
        hal.set_p('plasmac_config.config-disable',configDisable)
        self.idlePin = hal_glib.GPin(halcomp.newpin('program-is-idle', hal.HAL_BIT, hal.HAL_IN))
        hal.connect('plasmac_config.program-is-idle', 'plasmac:program-is-idle') 
        self.idlePin.connect('value-changed', self.idle_changed)
        self.thcFeedRate = (float(self.i.find('AXIS_Z', 'MAX_VELOCITY')) * \
                              float(self.i.find('AXIS_Z', 'OFFSET_AV_RATIO'))) * 60
        self.configFile = self.i.find('EMC', 'MACHINE').lower() + '_config.cfg'
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.configDict = {}
        self.oldMode = 9
        self.materialsUpdate = False
        self.maxHeight = hal.get_value('ini.z.max_limit') - hal.get_value('ini.z.min_limit')
        self.configure_widgets()
        self.builder.get_object('probe-feed-rate-adj').set_upper(self.builder.get_object('setup-feed-rate').get_value())
        self.load_settings()
        self.set_theme()
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
