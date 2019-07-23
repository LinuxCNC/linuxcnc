#!/usr/bin/env python

'''
plasmac_run.py

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

import os
import gtk
import linuxcnc
import gobject
import hal, hal_glib
from   gladevcp.persistence import widget_defaults,select_widgets
import gladevcp
from subprocess import Popen,PIPE
import time

class HandlerClass:

    def check_material_file(self):
        version = '[VERSION 1]'
        tempMaterialDict = {}
        if os.path.exists(self.materialFile):
            if not version in open(self.materialFile).read():
                print('*** upgrading material file if we needed to ...')
                # well, it should if we ever need it and we write the code :-)
                #with open(self.materialFile, 'r') as f_in:
                #    for line in f_in:
                #        if not line.strip().startswith('#') and len(line.strip()):
                #            read in old version
                #            convert to new version
                #            write new version
        else: # create a new material file if it doesn't exist
            with open(self.materialFile, 'w') as f_out:
                f_out.write(\
                    '#plasmac material file\n'\
                    '#the next line is required for version checking\n'\
                    + version + '\n\n'\
                    '#example only, may be deleted\n'\
                    '#[MATERIAL_NUMBER_1]  \n'\
                    '#NAME               = \n'\
                    '#KERF_WIDTH         = \n'\
                    '#THC                = (0 = off, 1 = on)\n'\
                    '#PIERCE_HEIGHT      = \n'\
                    '#PIERCE_DELAY       = \n'\
                    '#PUDDLE_JUMP_HEIGHT = (optional, set to 0 or delete if not required)\n'\
                    '#PUDDLE_JUMP_DELAY  = (optional, set to 0 or delete if not required)\n'\
                    '#CUT_HEIGHT         = \n'\
                    '#CUT_SPEED          = \n'\
                    '#CUT_AMPS           = (optional, only used for operator information)\n'\
                    '#CUT_VOLTS          = (modes 0 & 1 only, if not using auto voltage sampling)\n'\
                    '\n')
            print('*** creating new material configuration file, {}'.format(self.materialFile))

    def get_material(self):
        self.getMaterial = 1
        t_number = 0
        t_name = 'Default'
        k_width = self.builder.get_object('kerf-width').get_value()
        thc_enable = self.builder.get_object('thc-enable').get_active()
        p_height = self.builder.get_object('pierce-height').get_value()
        p_delay = self.builder.get_object('pierce-delay').get_value()
        pj_height = self.builder.get_object('puddle-jump-height').get_value()
        pj_delay = self.builder.get_object('puddle-jump-delay').get_value()
        c_height = self.builder.get_object('cut-height').get_value()
        c_speed = self.builder.get_object('cut-feed-rate').get_value()
        c_amps = self.builder.get_object('cut-amps').get_value()
        c_volts = self.builder.get_object('cut-volts').get_value()
        try:
            with open(self.materialFile, 'r') as f_in:
                self.builder.get_object('materials').clear()
                self.materialList = []
                for line in f_in:
                    if not line.startswith('#'):
                        if line.startswith('[MATERIAL_NUMBER_') and line.strip().endswith(']'):
                            self.materialFileDict[t_number] = [t_name, k_width, thc_enable, p_height, p_delay, pj_height, pj_delay, c_height, c_speed, c_amps, c_volts]
                            iter = self.builder.get_object('materials').append()
                            self.builder.get_object('materials').set(iter, 0, '{:05d}: {}'.format(int(t_number), t_name))
                            a,b,c = line.split('_')
                            t_number = int(c.replace(']',''))
                            t_name = 'none'
                            k_width = thc_enable = p_height = p_delay = pj_height = pj_delay = c_height = c_speed = c_amps = c_volts =  0
                        elif line.startswith('NAME'):
                            t_name = line.split('=')[1].strip()
                        elif line.startswith('KERF_WIDTH'):
                            k_width = float(line.split('=')[1].strip())
                            self.materialKerfMap[t_number] = k_width
                        elif line.startswith('THC'):
                            thc_enable = int(line.split('=')[1].strip())
                        elif line.startswith('PIERCE_HEIGHT'):
                            p_height = float(line.split('=')[1].strip())
                        elif line.startswith('PIERCE_DELAY'):
                            p_delay = float(line.split('=')[1].strip())
                        elif line.startswith('PUDDLE_JUMP_HEIGHT'):
                            pj_height = float(line.split('=')[1].strip())
                        elif line.startswith('PUDDLE_JUMP_DELAY'):
                            pj_delay = float(line.split('=')[1].strip())
                        elif line.startswith('CUT_HEIGHT'):
                            c_height = float(line.split('=')[1].strip())
                        elif line.startswith('CUT_SPEED'):
                            c_speed = float(line.split('=')[1].strip())
                        elif line.startswith('CUT_AMPS'):
                            c_amps = float(line.split('=')[1].strip())
                        elif line.startswith('CUT_VOLTS'):
                            c_volts = float(line.split('=')[1].strip())
                self.materialFileDict[t_number] = [t_name, k_width, thc_enable, p_height, p_delay, pj_height, pj_delay, c_height, c_speed, c_amps, c_volts]
                self.materialList.append(t_number)
                iter = self.builder.get_object('materials').append()
                self.builder.get_object('materials').set(iter, 0, '{:05d}: {}'.format(int(t_number), t_name))
        except:
            print('*** material file, {} is invalid'.format(self.materialFile))
        self.builder.get_object('material').set_active(0)

        self.materialList = []
        for material in self.materialKerfMap.keys():
            self.materialList.append(material)
        self.getMaterial = 0

    def on_save_clicked(self,widget,data=None):
        self.save_settings()
        self.materialFileDict[0][0] = self.materialName
        self.materialFileDict[0][1] = self.builder.get_object('kerf-width').get_value()
        self.materialFileDict[0][2] = self.builder.get_object('thc-enable').get_active()
        self.materialFileDict[0][3] = self.builder.get_object('pierce-height').get_value()
        self.materialFileDict[0][4] = self.builder.get_object('pierce-delay').get_value()
        self.materialFileDict[0][5] = self.builder.get_object('puddle-jump-height').get_value()
        self.materialFileDict[0][6] = self.builder.get_object('puddle-jump-delay').get_value()
        self.materialFileDict[0][7] = self.builder.get_object('cut-height').get_value()
        self.materialFileDict[0][8] = self.builder.get_object('cut-feed-rate').get_value()
        self.materialFileDict[0][9] = self.builder.get_object('cut-amps').get_value()
        self.materialFileDict[0][10] = self.builder.get_object('cut-volts').get_value()

    def on_reload_clicked(self,widget,data=None):
        self.materialUpdate = True
        self.load_settings()
        self.materialFileDict = {}
        self.materialKerfMap = {0: 0.0}
        self.get_material()
        self.materialUpdate = False

    def on_thc_auto_toggled(self,button):
        if button.get_active():
            self.builder.get_object('thc-enable').set_sensitive(True)
            self.builder.get_object('thc-enable-label').set_text('THC Enable')

    def on_thc_on_toggled(self,button):
        if button.get_active():
            self.halcomp['thc-enable-out'] = 1
            self.builder.get_object('thc-enable').set_sensitive(False)
            self.builder.get_object('thc-enable-label').set_text('THC ENABLED')

    def on_thc_off_toggled(self,button):
        if button.get_active():
            self.halcomp['thc-enable-out'] = 0

            self.builder.get_object('thc-enable').set_sensitive(False)
            self.builder.get_object('thc-enable-label').set_text('THC DISABLED')

    def on_material_changed(self,widget):
        if self.getMaterial: return
        material, name = self.builder.get_object('material').get_active_text().split(': ', 1)
        self.change_material(int(material))

    def material_change_number_changed(self,halpin):
        if self.getMaterial: return
        hal.set_p('motion.digital-in-03','0')
        material = halpin.get()
        if material in self.materialKerfMap.keys():
            self.change_material(int(material))
            hal.set_p('plasmac_run.material-change','1')
            hal.set_p('motion.digital-in-03','1')
        else:
            hal.set_p('plasmac_run.material-change','-1')

    def material_change_changed(self,halpin):
        if halpin.get() == 0:
            hal.set_p('motion.digital-in-03','0')

    def change_material(self,material):
        if material in self.materialKerfMap.keys():
            self.materialName = self.materialFileDict[material][0]
            self.builder.get_object('kerf-width').set_value(self.materialFileDict[material][1])
            self.builder.get_object('thc-enable').set_active(self.materialFileDict[material][2])
            self.builder.get_object('pierce-height').set_value(self.materialFileDict[material][3])
            self.builder.get_object('pierce-delay').set_value(self.materialFileDict[material][4])
            self.builder.get_object('puddle-jump-height').set_value(self.materialFileDict[material][5])
            self.builder.get_object('puddle-jump-delay').set_value(self.materialFileDict[material][6])
            self.builder.get_object('cut-height').set_value(self.materialFileDict[material][7])
            self.builder.get_object('cut-feed-rate').set_value(self.materialFileDict[material][8])
            self.builder.get_object('cut-amps').set_value(self.materialFileDict[material][9])
            self.builder.get_object('cut-volts').set_value(self.materialFileDict[material][10])
            hal.set_p('plasmac_run.material-change-number',str(material))
        else:
            print('material not in self.materialKerfMap.keys()')
#            if material < self.oldMaterial:
#                self.change_material(self.materialList[self.materialList.index(self.oldMaterial) - 1])
#            else:
#                self.change_material(self.materialList[self.materialList.index(self.oldMaterial) + 1])
        if material in self.materialList:
            self.builder.get_object('material').set_active(self.materialList.index(material))
        self.oldMaterial = material

    def on_setupFeedRate_value_changed(self, widget):
        self.builder.get_object('probe-feed-rate-adj').configure(self.builder.get_object('probe-feed-rate').get_value(),0,self.builder.get_object('setup-feed-rate').get_value(),1,0,0)

    def configure_widgets(self):
        # set_digits = number of digits after decimal
        # configure  = (value, lower limit, upper limit, step size, 0, 0)
        self.builder.get_object('cornerlock-enable').set_active(1)
        self.builder.get_object('cut-amps').set_digits(0)
        self.builder.get_object('cut-amps-adj').configure(45,0,999,1,0,0)
        self.builder.get_object('cut-volts').set_digits(1)
        self.builder.get_object('cut-volts-adj').configure(122,50,300,0.1,0,0)
        self.builder.get_object('kerfcross-enable').set_active(0)
        self.builder.get_object('ohmic-probe-enable').set_active(1)
        self.builder.get_object('pierce-delay').set_digits(1)
        self.builder.get_object('pierce-delay-adj').configure(0.5,0,10,0.1,0,0)
        self.builder.get_object('puddle-jump-height').set_digits(0)
        self.builder.get_object('puddle-jump-height-adj').configure(0,0,200,1,0,0)
        self.builder.get_object('puddle-jump-delay').set_digits(2)
        self.builder.get_object('puddle-jump-delay-adj').configure(0,0,9,0.01,0,0)
        self.builder.get_object('thc-enable').set_active(1)
        self.builder.get_object('use-auto-volts').set_active(1)
        if self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'mm':
            self.builder.get_object('kerf-width').set_digits(2)
            self.builder.get_object('kerf-width-adj').configure(0.5,0,5,0.01,0,0)
            self.builder.get_object('cut-feed-rate').set_digits(0)
            self.builder.get_object('cut-feed-rate-adj').configure(4000,0,9999,1,0,0)
            self.builder.get_object('cut-height').set_digits(2)
            self.builder.get_object('cut-height-adj').configure(1,0,25,0.01,0,0)
            self.builder.get_object('pierce-height').set_digits(2)
            self.builder.get_object('pierce-height-adj').configure(4,0,25,0.01,0,0)
        elif self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch':
            self.builder.get_object('kerf-width').set_digits(3)
            self.builder.get_object('kerf-width-adj').configure(0.02,0,0.25,1,0,0)
            self.builder.get_object('cut-feed-rate').set_digits(1)
            self.builder.get_object('cut-feed-rate-adj').configure(160,0,400,0.1,0,0)
            self.builder.get_object('cut-height').set_digits(3)
            self.builder.get_object('cut-height-adj').configure(0.04,0,1,0.001,0,0)
            self.builder.get_object('pierce-height').set_digits(3)
            self.builder.get_object('pierce-height-adj').configure(0.16,0,1,0.001,0,0)
        else:
            print('*** incorrect [TRAJ]LINEAR_UNITS in ini file')

    def periodic(self):
        if self.builder.get_object('thc-auto').get_active():
            self.halcomp['thc-enable-out'] = self.builder.get_object('thc-enable').get_active()

        mode = hal.get_value('plasmac.mode')
        if mode != self.oldMode:
            if mode == 0:
                self.builder.get_object('kerfcross-enable').show()
                self.builder.get_object('kerfcross-enable-label').show()
                self.builder.get_object('volts-box').show()
            elif mode == 1:
                self.builder.get_object('kerfcross-enable').show()
                self.builder.get_object('kerfcross-enable-label').show()
                self.builder.get_object('volts-box').show()
            elif mode == 2:
                self.builder.get_object('kerfcross-enable').hide()
                self.builder.get_object('kerfcross-enable-label').hide()
                self.builder.get_object('volts-box').hide()
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
            gtk.settings_get_default().set_property('gtk-font-name', font)
        gtk.settings_get_default().set_property('gtk-theme-name', theme)

    def load_settings(self):
        for item in widget_defaults(select_widgets(self.builder.get_objects(), hal_only=False,output_only = True)):
            self.configDict[item] = '0'
        convertFile = False
        if os.path.exists(self.configFile):
            try:
                tmpDict = {}
                with open(self.configFile, 'r') as f_in:
                    for line in f_in:
                        if not line.startswith('#') and not line.startswith('[') and not line.startswith('\n'):
                            if 'version' in line or 'signature' in line:
                                convertFile = True
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
                print('*** plasmac configuration file, {} is invalid ***'.format(self.configFile))
            for item in self.configDict:
                if item == 'material-number' or item == 'kerf-width':
                    self.builder.get_object(item).set_value(0)
                elif isinstance(self.builder.get_object(item), gladevcp.hal_widgets.HAL_SpinButton):
                    if item in tmpDict:
                        self.builder.get_object(item).set_value(float(self.configDict.get(item)))
                    else:
                        print('*** {} missing from {}'.format(item,self.configFile))
                elif isinstance(self.builder.get_object(item), gladevcp.hal_widgets.HAL_CheckButton):
                    if item in tmpDict:
                        self.builder.get_object(item).set_active(int(self.configDict.get(item)))
                    else:
                        print('*** {} missing from {}'.format(item,self.configFile))
            if convertFile:
                print('*** converting {} to new format'.format(self.configFile))
                self.save_settings()
        else:
            self.save_settings()
            print('*** creating new run tab configuration file, {}',format(self.configFile))

    def save_settings(self):
        try:
            with open(self.configFile, 'w') as f_out:
                f_out.write('#plasmac run tab/panel configuration file, format is:\n#name = value\n\n')
                for key in sorted(self.configDict.iterkeys()):
                    if key == 'material-number' or key == 'kerf-width':
                        pass
                    elif isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_SpinButton):
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

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.materialNumberPin = hal_glib.GPin(halcomp.newpin('material-change-number', hal.HAL_S32, hal.HAL_IN))
        self.materialChangePin = hal_glib.GPin(halcomp.newpin('material-change', hal.HAL_S32, hal.HAL_IN))
        self.thcEnablePin = hal_glib.GPin(halcomp.newpin('thc-enable-out', hal.HAL_BIT, hal.HAL_OUT))
        self.materialNumberPin.connect('value-changed', self.material_change_number_changed)
        self.materialChangePin.connect('value-changed', self.material_change_changed)
        self.thcFeedRate = (float(self.i.find('AXIS_Z', 'MAX_VELOCITY')) * \
                              float(self.i.find('AXIS_Z', 'OFFSET_AV_RATIO'))) * 60
        hal.set_p('plasmac.thc-feed-rate','{}'.format(self.thcFeedRate))
        self.configFile = self.i.find('EMC', 'MACHINE').lower() + '_run.cfg'
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.materialFile = self.i.find('EMC', 'MACHINE').lower() + '_material.cfg'
        self.materialFileDict = {}
        self.materialKerfMap = {0: 0.0}
        self.materialDict = {}
        self.configDict = {}
        hal.set_p('plasmac.mode','{}'.format(int(self.i.find('PLASMAC','MODE') or '0')))
        self.oldMode = 9
        self.oldMaterial = -1
        self.materialUpdate = False
        self.configure_widgets()
        self.load_settings()
        self.check_material_file()
        self.get_material()
        self.set_theme()
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
