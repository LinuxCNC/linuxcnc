#!/usr/bin/env python

'''
plasmac_run.py

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
import sys
import gtk
import linuxcnc
import gobject
import hal, hal_glib
import gladevcp
import time
import shutil
from gladevcp.persistence import widget_defaults,select_widgets
from subprocess import Popen,PIPE

class HandlerClass:

    def dialog_error(self, title, error):
        md = gtk.MessageDialog(self.W,
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_ERROR,
                               gtk.BUTTONS_CLOSE,
                               error)
        md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        md.set_keep_above(True)
        md.set_title(title)
        md.run()
        md.destroy()

    def dialog_ok_cancel(self,title,text,button1Txt,button2Txt):
        md = gtk.Dialog(title,
                        self.W,
                        gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                        (button1Txt, 1, button2Txt, 0))
        md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        md.set_keep_above(True)
        label = gtk.Label(text)
        md.vbox.add(label)
        label.show()
        response = md.run()
        md.destroy()
        return response

    def dialog_common(self,title,label1Txt,label2Txt,button1Txt,button2Txt):
        md = gtk.Dialog(title,
                        self.W,
                        gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                        (button1Txt, 1, button2Txt, 0))
        md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        md.set_keep_above(True)
        label1 = gtk.Label(label1Txt)
        md.vbox.add(label1)
        entry1 = gtk.Entry()
        md.vbox.add(entry1)
        label2 = gtk.Label(label2Txt)
        md.vbox.add(label2)
        entry2 = gtk.Entry()
        md.vbox.add(entry2)
        if label1Txt:
            label1.show()
            entry1.show()
        if label2Txt:
            label2.show()
            entry2.show()
        reply1 = reply2 = ''
        response = md.run()
        if response:
            if label1:
                reply1 = entry1.get_text()
            if label1:
                reply2 = entry2.get_text()
        md.destroy()
        return response, reply1, reply2

    def check_material_file(self):
        tempMaterialDict = {}
        # create a new material file if it doesn't exist
        if not os.path.exists(self.materialFile):
            with open(self.materialFile, 'w') as f_out:
                f_out.write(\
                    '# plasmac material file\n'\
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
                    '\n')
            print('*** creating new material configuration file, {}'.format(self.materialFile))

    def write_materials(self, *items):
        mat = []
        for item in items[1:]:
            mat.append(item)
        self.materialFileDict[items[0]] = mat

    def display_materials(self):
        self.materialList = []
        self.builder.get_object('materials').clear()
        for key in sorted(self.materialFileDict):
            iter = self.builder.get_object('materials').append()
            self.builder.get_object('materials').set(iter, 0, '{:05d}: {}'.format(key, self.materialFileDict[key][0]))
            self.materialList.append(key)

    def get_material(self):
        self.getMaterialBusy = 1
        self.builder.get_object('materials').clear()
        t_number = 0
        t_name = 'Default'
        self.materialName = t_name
        k_width = (self.builder.get_object('kerf-width').get_value())
        thc_enable = self.builder.get_object('thc-enable').get_active()
        p_height = self.builder.get_object('pierce-height').get_value()
        p_delay = self.builder.get_object('pierce-delay').get_value()
        pj_height = self.builder.get_object('puddle-jump-height').get_value()
        pj_delay = self.builder.get_object('puddle-jump-delay').get_value()
        c_height = self.builder.get_object('cut-height').get_value()
        c_speed = self.builder.get_object('cut-feed-rate').get_value()
        c_amps = self.builder.get_object('cut-amps').get_value()
        c_volts = self.builder.get_object('cut-volts').get_value()
        pause = self.builder.get_object('pause-at-end').get_value()
        g_press = self.builder.get_object('gas-pressure').get_value()
        c_mode = self.builder.get_object('cut-mode').get_value()
        t_item = 0
        self.write_materials(t_number,t_name,k_width,thc_enable,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
        with open(self.materialFile, 'r') as f_in:
            firstpass = True
            required = ['PIERCE_HEIGHT', 'PIERCE_DELAY', 'CUT_HEIGHT', 'CUT_SPEED']
            received = []
            for line in f_in:
                if line.startswith('#'):
                    continue
                elif line.startswith('[MATERIAL_NUMBER_') and line.strip().endswith(']'):
                    newMaterial = True
                    if not firstpass:
                        self.write_materials(t_number,t_name,k_width,thc_enable,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                        for item in required:
                            if item not in received:
                                self.dialog_error('Materials Error', '\n{} is missing from Material #{}'.format(item, t_number))
                    firstpass = False
                    t_number = int(line.rsplit('_', 1)[1].strip().strip(']'))
                    self.materialNumList.append(t_number)
                    t_name = k_width = thc_enable = p_height = p_delay = pj_height = pj_delay = c_height = c_speed = c_amps = c_volts =  pause = g_press = c_mode = 0.0
                    t_item += 1
                    received = []
                elif line.startswith('NAME'):
                    if line.split('=')[1].strip():
                        t_name = line.split('=')[1].strip()
                elif line.startswith('KERF_WIDTH'):
                    if line.split('=')[1].strip():
                        k_width = float(line.split('=')[1].strip())
                elif line.startswith('THC'):
                    if line.split('=')[1].strip():
                        thc_enable = int(line.split('=')[1].strip())
                elif line.startswith('PIERCE_HEIGHT'):
                    received.append('PIERCE_HEIGHT')
                    if line.split('=')[1].strip():
                        p_height = float(line.split('=')[1].strip())
                    elif t_number:
                        self.dialog_error('Materials Error', '\nNo value for PIERCE_HEIGHT in Material #{}'.format(t_number))
                elif line.startswith('PIERCE_DELAY'):
                    received.append('PIERCE_DELAY')
                    if line.split('=')[1].strip():
                        p_delay = float(line.split('=')[1].strip())
                    else:
                        self.dialog_error('Materials Error', '\nNo value for PIERCE_DELAY in Material #{}'.format(t_number))
                elif line.startswith('PUDDLE_JUMP_HEIGHT'):
                    if line.split('=')[1].strip():
                        pj_height = float(line.split('=')[1].strip())
                elif line.startswith('PUDDLE_JUMP_DELAY'):
                    if line.split('=')[1].strip():
                        pj_delay = float(line.split('=')[1].strip())
                elif line.startswith('CUT_HEIGHT'):
                    received.append('CUT_HEIGHT')
                    if line.split('=')[1].strip():
                        c_height = float(line.split('=')[1].strip())
                    else:
                        self.dialog_error('Materials Error', '\nNo value for CUT_HEIGHT in Material #{}'.format(t_number))
                elif line.startswith('CUT_SPEED'):
                    received.append('CUT_SPEED')
                    if line.split('=')[1].strip():
                        c_speed = float(line.split('=')[1].strip())
                    else:
                        self.dialog_error('Materials Error', '\nNo value for CUT_SPEED in Material #{}'.format(t_number))
                elif line.startswith('CUT_AMPS'):
                    if line.split('=')[1].strip():
                        c_amps = float(line.split('=')[1].strip().replace(' ',''))
                elif line.startswith('CUT_VOLTS'):
                    if line.split('=')[1].strip():
                        c_volts = float(line.split('=')[1].strip())
                elif line.startswith('PAUSE_AT_END'):
                    if line.split('=')[1].strip():
                        pause = float(line.split('=')[1].strip())
                elif line.startswith('GAS_PRESSURE'):
                    if line.split('=')[1].strip():
                        g_press = float(line.split('=')[1].strip())
                elif line.startswith('CUT_MODE'):
                    if line.split('=')[1].strip():
                        c_mode = float(line.split('=')[1].strip())
            if t_number:
                self.write_materials(t_number,t_name,k_width,thc_enable,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                for item in required:
                    if item not in received:
                        self.dialog_error('Materials Error', '\n{} is missing from Material #{}'.format(item, t_number))
        self.display_materials()
        self.builder.get_object('material').set_active(0)
        self.getMaterialBusy = 0

    def on_save_clicked(self,widget,data=None):
        self.save_settings()
        active = int(self.builder.get_object('material').get_active_text().split(': ', 1)[0])
        self.materialFileDict[active][0] = self.materialName
        self.materialFileDict[active][1] = self.builder.get_object('kerf-width').get_value()
        self.materialFileDict[active][2] = self.builder.get_object('thc-enable').get_active()
        self.materialFileDict[active][3] = self.builder.get_object('pierce-height').get_value()
        self.materialFileDict[active][4] = self.builder.get_object('pierce-delay').get_value()
        self.materialFileDict[active][5] = self.builder.get_object('puddle-jump-height').get_value()
        self.materialFileDict[active][6] = self.builder.get_object('puddle-jump-delay').get_value()
        self.materialFileDict[active][7] = self.builder.get_object('cut-height').get_value()
        self.materialFileDict[active][8] = self.builder.get_object('cut-feed-rate').get_value()
        self.materialFileDict[active][9] = self.builder.get_object('cut-amps').get_value()
        self.materialFileDict[active][10] = self.builder.get_object('cut-volts').get_value()
        self.materialFileDict[active][11] = self.builder.get_object('pause-at-end').get_value()
        self.materialFileDict[active][12] = self.builder.get_object('gas-pressure').get_value()
        self.materialFileDict[active][13] = self.builder.get_object('cut-mode').get_value()

    def on_reload_clicked(self,widget,data=None):
        self.materialUpdate = True
        material = self.builder.get_object('material').get_active()
        if widget:
            self.load_settings()
        self.materialFileDict = {}
        self.materialNumList = []
        self.get_material()
        if material not in self.materialFileDict:
            material = 0
        self.builder.get_object('material').set_active(material)
        self.materialUpdate = False
        hal.set_p('plasmac_run.material-reload', '0')

    def on_material_reload_pin(self, halpin):
        if halpin.get():
            self.on_reload_clicked(1)

    def on_temp_material_pin(self, halpin):
        if halpin.get():
            t_number = 0
            t_name = 'Temporary'
            t_item = 0
            with open(self.tmpMaterialFile, 'r') as f_in:
                for line in f_in:
                    if line.startswith('kerf-width'):
                        k_width = float(line.split('=')[1].strip()) 
                    elif line.startswith('thc-enable'):
                        thc_enable = int(line.split('=')[1].strip())
                    elif line.startswith('pierce-height'):
                        p_height = float(line.split('=')[1].strip())
                    elif line.startswith('pierce-delay'):
                        p_delay = float(line.split('=')[1].strip())
                    elif line.startswith('puddle-jump-height'):
                        pj_height = float(line.split('=')[1].strip())
                    elif line.startswith('puddle-jump-delay'):
                        pj_delay = float(line.split('=')[1].strip())
                    elif line.startswith('cut-height'):
                        c_height = float(line.split('=')[1].strip())
                    elif line.startswith('cut-feed-rate'):
                        c_speed = float(line.split('=')[1].strip())
                    elif line.startswith('cut-amps'):
                        c_amps = float(line.split('=')[1].strip())
                    elif line.startswith('cut-volts'):
                        c_volts = float(line.split('=')[1].strip())
                    elif line.startswith('pause-at-end'):
                        pause = float(line.split('=')[1].strip())
                    elif line.startswith('gas-pressure'):
                        g_press = float(line.split('=')[1].strip())
                    elif line.startswith('cut-mode'):
                        c_mode = float(line.split('=')[1].strip())
            self.write_materials(t_number,t_name,k_width,thc_enable,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
            self.display_materials()
            self.change_material(0)
            self.builder.get_object('material').set_active(0)
            hal.set_p('plasmac_run.temp-material', '0')

    def on_new_clicked(self, widget):
        response, num, nam = self.dialog_common('Add Material',\
                                                'New Material Number',\
                                                'Material Name',\
                                                'OK',\
                                                'Cancel')
        if not num or not nam:
            self.dialog_error('New Material Error','\nNumber and Name are required')
            return
        else:
            try:
                num = int(num)
            except:
                self.dialog_error('New Material Error','\nMaterial number must be an integer')
                return
            if num in self.materialNumList:
                self.dialog_error('New Material Error','\nMaterial number {} is in use'.format(num))
                return
            active = self.builder.get_object('material').get_active_text().split(': ', 1)[0].lstrip('0')
            active = int(active) if active else 0
            shutil.copy(self.materialFile,'{}.bkp'.format(self.materialFile))
            outFile = open('{}'.format(self.materialFile), 'a')
            outFile.write('[MATERIAL_NUMBER_{}]  \n'.format(num))
            outFile.write('NAME               = {}\n'.format(nam))
            outFile.write('KERF_WIDTH         = {}\n'.format(self.materialFileDict[active][1]))
            if self.materialFileDict[0][2] == True:
                thc = 1
            else:
                thc = 0
            if self.materialFileDict[active][2]:
                outFile.write('THC                = 1\n')
            else:
                outFile.write('THC                = 0\n')
            outFile.write('PIERCE_HEIGHT      = {}\n'.format(self.materialFileDict[active][3]))
            outFile.write('PIERCE_DELAY       = {}\n'.format(self.materialFileDict[active][4]))
            outFile.write('PUDDLE_JUMP_HEIGHT = {}\n'.format(self.materialFileDict[active][5]))
            outFile.write('PUDDLE_JUMP_DELAY  = {}\n'.format(self.materialFileDict[active][6]))
            outFile.write('CUT_HEIGHT         = {}\n'.format(self.materialFileDict[active][7]))
            outFile.write('CUT_SPEED          = {}\n'.format(self.materialFileDict[active][8]))
            outFile.write('CUT_AMPS           = {}\n'.format(self.materialFileDict[active][9]))
            outFile.write('CUT_VOLTS          = {}\n'.format(self.materialFileDict[active][10]))
            outFile.write('PAUSE_AT_END       = {}\n'.format(self.materialFileDict[active][11]))
            outFile.write('GAS_PRESSURE       = {}\n'.format(self.materialFileDict[active][12]))
            outFile.write('CUT_MODE           = {}\n\n'.format(self.materialFileDict[active][13]))
            outFile.close()
            self.materialUpdate = True
            self.load_settings()
            self.materialFileDict = {}
            self.materialNumList = []
            self.get_material()
            self.builder.get_object('material').set_active(self.materialList.index(num))
            self.materialUpdate = False

    def on_delete_clicked(self, widget):
        response, num, nam = self.dialog_common('Delete Material',\
                                                'Material Number To Delete',\
                                                '',\
                                                'OK',\
                                                'Cancel')
        if not num:
            self.dialog_error('Delete Material Error','\nNumber is required')
            return
        else:
            try:
                num = int(num)
            except:
                self.dialog_error('Delete Material Error','\nMaterial number must be an integer')
                return
            if not num in self.materialNumList:
                self.dialog_error('Delete Material Error','\nMaterial number {} is not in use'.format(num))
                return
            response = self.dialog_ok_cancel('Delete Material',\
                                             'Are you sure?',\
                                             'OK',\
                                             'Cancel')
            if response == 0:
                return
            shutil.copy(self.materialFile,'{}.bkp'.format(self.materialFile))
            inFile = open('{}.bkp'.format(self.materialFile), 'r')
            outFile = open('{}'.format(self.materialFile), 'w')
            while 1:
                line = inFile.readline()
                if not line: break
                elif line.startswith('[MATERIAL_NUMBER_') and \
                     int(line.strip().strip(']').split('[MATERIAL_NUMBER_')[1]) == num:
                    break
                else:
                    outFile.write(line)
            while 1:
                line = inFile.readline()
                if not line: break
                elif line.startswith('[MATERIAL_NUMBER_'):
                    outFile.write(line)
                    break
            while 1:
                line = inFile.readline()
                if not line: break
                else:
                    outFile.write(line)
            outFile.close()
            self.materialUpdate = True
            self.load_settings()
            self.materialFileDict = {}
            self.materialNumList = []
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

    def first_material_changed(self, halpin):
        material = halpin.get()
        if not self.material_exists(material):
            return
        self.builder.get_object('material').set_active(self.materialList.index(material))

    def material_change_number_changed(self,halpin):
        if self.getMaterialBusy:
            return
        material = int(halpin.get())
        oldMaterial = int(self.builder.get_object('material').get_active_text().split(': ', 1)[0])
        if hal.get_value('plasmac_run.material-change') == 1:
            self.autoChange = True
            # material already loaded so do a phantom handshake
            if material < 0:
                hal.set_p('plasmac_run.material-change','2')
                hal.set_p('motion.digital-in-03','1')
                hal.set_p('plasmac_run.material-change-number','{}'.format(material * -1))
                return
        # does material exist
        if not self.material_exists(material):
            self.autoChange = False
            return
        self.builder.get_object('material').set_active(self.materialList.index(material))

    def material_exists(self, material):
        if int(material) in self.materialList:
            return True
        else:
            if self.autoChange:
                hal.set_p('plasmac_run.material-change','-1')
                hal.set_p('plasmac_run.material-change-number', '{}'.format(int(self.builder.get_object('material').get_active_text().split(': ', 1)[0])))
            self.dialog_error('Materials Error', '\nMaterial #{} not in material list'.format(int(material)))
            return False

    def on_material_changed(self,widget):
        if widget.get_active_text():
            if self.getMaterialBusy:
                hal.set_p('plasmac_run.material-change','0')
                self.autoChange = False
                return
            material = int(widget.get_active_text().split(': ', 1)[0])
            if self.autoChange:
                hal.set_p('motion.digital-in-03','0')
                self.change_material(material)
                hal.set_p('plasmac_run.material-change','2')
                hal.set_p('motion.digital-in-03','1')
            else:
                self.change_material(material)
        self.autoChange = False

    def material_change_changed(self, halpin):
        if halpin.get() == 0:
            hal.set_p('motion.digital-in-03','0')

    def material_change_timeout(self, halpin):
        if halpin.get():
            material = int(self.builder.get_object('material').get_active_text().split(': ', 1)[0])
#           FIX_ME do we need to stop the program if a timeout occurs???
            print('\nMaterial change timeout occured for material #{}'.format(material))
            hal.set_p('plasmac_run.material-change-number', '{}'.format(material))
            hal.set_p('plasmac_run.material-change-timeout', '0')
            hal.set_p('motion.digital-in-03','0')

    def change_material(self, material):
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
            self.builder.get_object('pause-at-end').set_value(self.materialFileDict[material][11])
            self.builder.get_object('gas-pressure').set_value(self.materialFileDict[material][12])
            self.builder.get_object('cut-mode').set_value(self.materialFileDict[material][13])
            hal.set_p('plasmac_run.material-change-number',str(material))

    def on_setupFeedRate_value_changed(self, widget):
        self.builder.get_object('probe-feed-rate-adj').configure(self.builder.get_object('probe-feed-rate').get_value(),0,self.builder.get_object('setup-feed-rate').get_value(),1,0,0)

    def on_single_cut_pressed(self, widget):
        self.builder.get_object('x-single-cut').update()
        self.builder.get_object('y-single-cut').update()
        x = self.builder.get_object('x-single-cut').get_value()
        y = self.builder.get_object('y-single-cut').get_value()
        if x <> 0 or y <> 0:
            self.s.poll()
            if not self.s.estop and self.s.enabled and self.s.homed.count(1) == self.s.joints and self.s.interp_state == linuxcnc.INTERP_IDLE:
                self.c.mode(linuxcnc.MODE_MDI)
                self.c.wait_complete()
                self.c.mdi('M3 $0 S1')
                self.c.mdi('G91')
                self.c.mdi('G1 X{} Y{} F#<_hal[plasmac.cut-feed-rate]>'.format(x, y))
                self.c.wait_complete()
                self.c.mdi('G90')
                self.c.mdi('M5')
                self.c.wait_complete()
                self.c.mdi('M30')
            else:
                print('current mode prevents a single cut')

    def configure_widgets(self):
        # set_digits = number of digits after decimal
        # configure  = (value, lower limit, upper limit, step size, 0, 0)
        self.builder.get_object('cornerlock-enable').set_active(1)
        self.builder.get_object('cut-amps').set_digits(0)
        self.builder.get_object('cut-amps-adj').configure(45,0,999,1,0,0)
        self.builder.get_object('cut-mode').set_digits(0)
        self.builder.get_object('cut-mode-adj').configure(1,1,3,1,0,0)
        self.builder.get_object('cut-volts').set_digits(1)
        self.builder.get_object('cut-volts-adj').configure(122,50,300,0.1,0,0)
        self.builder.get_object('gas-pressure').set_digits(0)
        self.builder.get_object('gas-pressure-adj').configure(0,0,0,1,0,0)
        self.builder.get_object('kerfcross-enable').set_active(0)
        self.builder.get_object('ohmic-probe-enable').set_active(1)
        self.builder.get_object('pause-at-end').set_digits(1)
        self.builder.get_object('pause-at-end-adj').configure(0,0,9,0.1,0,0)
        self.builder.get_object('pierce-delay').set_digits(1)
        self.builder.get_object('pierce-delay-adj').configure(0.0,0,10,0.1,0,0)
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
            self.builder.get_object('cut-feed-rate-adj').configure(4000,0,19999,1,0,0)
            self.builder.get_object('cut-height').set_digits(2)
            self.builder.get_object('cut-height-adj').configure(1,0,25,0.01,0,0)
            self.builder.get_object('pierce-height').set_digits(2)
            self.builder.get_object('pierce-height-adj').configure(4,0,25,0.01,0,0)
            self.builder.get_object('x-single-cut').set_digits(0)
            self.builder.get_object('x-single-cut-adj').configure(0,-9999,9999,1,0,0)
            self.builder.get_object('y-single-cut').set_digits(0)
            self.builder.get_object('y-single-cut-adj').configure(0,-9999,9999,1,0,0)
        elif self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch':
            self.builder.get_object('kerf-width').set_digits(4)
            self.builder.get_object('kerf-width-adj').configure(0.02,0,1,0.0001,0,0)
            self.builder.get_object('cut-feed-rate').set_digits(1)
            self.builder.get_object('cut-feed-rate-adj').configure(160,0,999,0.1,0,0)
            self.builder.get_object('cut-height').set_digits(3)
            self.builder.get_object('cut-height-adj').configure(0.04,0,1,0.001,0,0)
            self.builder.get_object('pierce-height').set_digits(3)
            self.builder.get_object('pierce-height-adj').configure(0.16,0,1,0.001,0,0)
            self.builder.get_object('x-single-cut').set_digits(4)
            self.builder.get_object('x-single-cut-adj').configure(0,-999,999,0.0625,0,0)
            self.builder.get_object('y-single-cut').set_digits(4)
            self.builder.get_object('y-single-cut-adj').configure(0,-999,999,0.0625,0,0)
        else:
            self.dialog_error('Configuration Error', 'incorrect [TRAJ]LINEAR_UNITS in ini file')
            print('*** incorrect [TRAJ]LINEAR_UNITS in ini file')

    def set_theme(self):
        theme = gtk.settings_get_default().get_property('gtk-theme-name')
        if os.path.exists(self.prefFile):
            try:
                with open(self.prefFile, 'r') as f_in:
                    for line in f_in:
                        if 'gtk_theme' in line and not 'Follow System Theme' in line:
                            (item, theme) = line.strip().replace(" ", "").split('=')
            except:
                self.dialog_error('Configuration Error', 'Preferences file, {} is invalid ***'.format(self.prefFile))
                print('*** preferences file, {} is invalid ***'.format(self.prefFile))
        else:
            theme = self.i.find('PLASMAC', 'THEME') or gtk.settings_get_default().get_property('gtk-theme-name')
            font = self.i.find('PLASMAC', 'FONT') or gtk.settings_get_default().get_property('gtk-font-name')
            fSize = int(font.split()[1])
#            font = '{} {}'.format(font.split()[0],fSize - 1 if fSize < 12 else fSize - 2)
            font = '{} {}'.format(font.split()[0],fSize - 1)
            gtk.settings_get_default().set_property('gtk-font-name', font)
        gtk.settings_get_default().set_property('gtk-theme-name', theme)

    def load_settings(self):
        for item in widget_defaults(select_widgets(self.builder.get_objects(), hal_only=True,output_only = True)):
            self.configDict[item] = '0'
        self.configDict['thc-mode'] = '0'
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
                self.dialog_error('Configuration Error', 'The plasmac configuration file, {} is invalid ***'.format(self.configFile))
                print('*** plasmac configuration file, {} is invalid ***'.format(self.configFile))
            for item in self.configDict:
                if item == 'material':
                    self.builder.get_object(item).set_active(0)
                elif item == 'thc-mode':
                    if self.configDict.get(item) != '0':
                        self.builder.get_object(self.configDict.get(item)).set_active(1)
                    else:
                        print('*** {} missing from {}'.format(item,self.configFile))
                elif isinstance(self.builder.get_object(item), gladevcp.hal_widgets.HAL_SpinButton):
                    if item in tmpDict:
                        self.builder.get_object(item).set_value(float(self.configDict.get(item)))
                    else:
                        if self.i.find('PLASMAC', 'PM_PORT'):
                                print('*** {} missing from {}'.format(item,self.configFile))
                        elif not item in ['gas-pressure', 'cut-mode']:
                            print('*** {} missing from {}'.format(item,self.configFile))
                elif isinstance(self.builder.get_object(item), gladevcp.hal_widgets.HAL_CheckButton):
                    if item in tmpDict:
                        # keep pmx485 alive if it was on when reload pressed
                        if item in ['powermax-enable'] and self.builder.get_object('powermax-enable').get_active():
                            self.builder.get_object(item).set_active(1)
                        else:
                            self.builder.get_object(item).set_active(int(self.configDict.get(item)))
                    else:
                        if self.i.find('PLASMAC', 'PM_PORT'):
                            print('*** {} missing from {}'.format(item,self.configFile))
                        elif not item in ['powermax-enable']:
                            print('*** {} missing from {}'.format(item,self.configFile))
            if convertFile:
                print('*** converting {} to new format'.format(self.configFile))
                self.save_settings()
        else:
            self.save_settings()
            print('*** creating new run tab configuration file, {}'.format(self.configFile))

    def save_settings(self):
        material = hal.get_value('plasmac_run.material-change-number')
        position = self.builder.get_object('material').get_active()
        if material == 0:
            try:
                with open(self.configFile, 'w') as f_out:
                    f_out.write('#plasmac run tab/panel configuration file, format is:\n#name = value\n\n')
                    for key in sorted(self.configDict.iterkeys()):
                        if key == 'material-number': # or key == 'kerf-width':
                            pass
                        elif isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_SpinButton):
                            self.builder.get_object(key).update()
                            value = self.builder.get_object(key).get_value()
                            f_out.write(key + '=' + str(value) + '\n')
                        elif isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_CheckButton):
                            value = self.builder.get_object(key).get_active()
                            f_out.write(key + '=' + str(value) + '\n')
                        elif key == 'torchPulseTime':
                            value = self.builder.get_object(key).get_value()
                            f_out.write(key + '=' + str(value) + '\n')
                        elif key == 'thc-mode':
                            if self.builder.get_object('thc-auto').get_active():
                                f_out.write(key + '=thc-auto\n')
                            if self.builder.get_object('thc-on').get_active():
                                f_out.write(key + '=thc-on\n')
                            if self.builder.get_object('thc-off').get_active():
                                f_out.write(key + '=thc-off\n')
            except:
                self.dialog_error('Config File Error', 'Error opening {}'.format(self.configFile))
                print('*** error opening {}'.format(self.configFile))
        if material != 0:
            for key in sorted(self.configDict.iterkeys()):
                if isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_SpinButton):
                    self.builder.get_object(key).update()
            shutil.copy(self.materialFile,'{}.tmp'.format(self.materialFile))
            inFile = open('{}.tmp'.format(self.materialFile), 'r')
            outFile = open('{}'.format(self.materialFile), 'w')
            while 1:
                line = inFile.readline()
                if not line: break
                elif line.startswith('[MATERIAL_NUMBER_') and \
                     material == int(line.strip().strip(']').split('[MATERIAL_NUMBER_')[1]):
                    outFile.write(line)
                    break
                else:
                    outFile.write(line)
            while 1:
                line = inFile.readline()
                if not line: break
                elif line.startswith('[MATERIAL_NUMBER_'):
                    outFile.write(line)
                    break
                elif line.startswith('NAME'):
                    outFile.write(line)
                elif line.startswith('KERF_WIDTH'):
                    outFile.write('KERF_WIDTH         = {}\n'.format(self.builder.get_object('kerf-width').get_value()))
                elif line.startswith('THC'):
                    if self.builder.get_object('thc-enable').get_active():
                        thc = 1
                    else:
                        thc = 0
                    outFile.write('THC                = {}\n'.format(thc))
                elif line.startswith('PIERCE_HEIGHT'):
                    outFile.write('PIERCE_HEIGHT      = {}\n'.format(self.builder.get_object('pierce-height').get_value()))
                elif line.startswith('PIERCE_DELAY'):
                    outFile.write('PIERCE_DELAY       = {}\n'.format(self.builder.get_object('pierce-delay').get_value()))
                elif line.startswith('PUDDLE_JUMP_HEIGHT'):
                    outFile.write('PUDDLE_JUMP_HEIGHT = {}\n'.format(self.builder.get_object('puddle-jump-height').get_value()))
                elif line.startswith('PUDDLE_JUMP_DELAY'):
                    outFile.write('PUDDLE_JUMP_DELAY  = {}\n'.format(self.builder.get_object('puddle-jump-delay').get_value()))
                elif line.startswith('CUT_HEIGHT'):
                    outFile.write('CUT_HEIGHT         = {}\n'.format(self.builder.get_object('cut-height').get_value()))
                elif line.startswith('CUT_SPEED'):
                    outFile.write('CUT_SPEED          = {}\n'.format(self.builder.get_object('cut-feed-rate').get_value()))
                elif line.startswith('CUT_AMPS'):
                    outFile.write('CUT_AMPS           = {}\n'.format(self.builder.get_object('cut-amps').get_value()))
                elif line.startswith('CUT_VOLTS'):
                    outFile.write('CUT_VOLTS          = {}\n'.format(self.builder.get_object('cut-volts').get_value()))
                elif line.startswith('PAUSE_AT_END'):
                    outFile.write('PAUSE_AT_END       = {}\n'.format(self.builder.get_object('pause-at-end').get_value()))
                elif line.startswith('GAS_PRESSURE'):
                    outFile.write('GAS_PRESSURE       = {}\n'.format(self.builder.get_object('gas-pressure').get_value()))
                elif line.startswith('CUT_MODE'):
                    outFile.write('CUT_MODE           = {}\n'.format(self.builder.get_object('cut-mode').get_value()))
                else:
                     outFile.write(line)
            while 1:
                line = inFile.readline()
                if not line: break
                outFile.write(line)
            inFile.close()
            outFile.close()
            self.materialUpdate = True
            self.load_settings()
            self.materialFileDict = {}
            self.get_material()
            self.builder.get_object('material').set_active(position)
            self.materialUpdate = False

    def periodic(self):
        if self.builder.get_object('thc-auto').get_active():
            self.halcomp['thc-enable-out'] = self.builder.get_object('thc-enable').get_active()
        mode = hal.get_value('plasmac.mode')
        if mode != self.oldMode:
            if mode == 0:
                self.builder.get_object('kerfcross-enable').show()
                self.builder.get_object('kerfcross-enable-label').show()
                self.builder.get_object('volts-box').show()
                self.builder.get_object('use-auto-volts').show()
                self.builder.get_object('use-auto-volts-label').show()
            elif mode == 1:
                self.builder.get_object('kerfcross-enable').show()
                self.builder.get_object('kerfcross-enable-label').show()
                self.builder.get_object('volts-box').show()
                self.builder.get_object('use-auto-volts').show()
                self.builder.get_object('use-auto-volts-label').show()
            elif mode == 2:
                self.builder.get_object('kerfcross-enable').hide()
                self.builder.get_object('kerfcross-enable-label').hide()
                self.builder.get_object('volts-box').hide()
                self.builder.get_object('use-auto-volts').hide()
                self.builder.get_object('use-auto-volts-label').hide()
            else:
                pass
            self.oldMode = mode
        self.s.poll()
        homed = True
        for n in range(self.s.joints):
            if not self.s.homed[n]:
                homed = False
        hal.set_p('plasmac.homed', str(homed))
        if homed and self.s.interp_state == linuxcnc.INTERP_IDLE:
            self.builder.get_object('single-cut').set_sensitive(True)
        else:
            self.builder.get_object('single-cut').set_sensitive(False)
        # for powermax communications
        if self.pmx485Started:
            if hal.component_exists('pmx485'):
                if not hal.get_value('pmx485.status'):
                    self.fault = '0000'
                    if self.pmx485Connected:
                        self.builder.get_object('powermax-label').set_text('Comms Error')
                        self.builder.get_object('powermax-label').set_tooltip_text('status of powermax communications')
                        self.builder.get_object('powermax-label').modify_fg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 1.0))
                        self.dialog_error('Communications Error', '\nPowermax communications error\n\nCheck cables and connections\n')
                    else:
                        if not self.builder.get_object('powermax-label').get_text() == 'Connecting':
                            self.builder.get_object('powermax-label').set_tooltip_text('status of powermax communications')
                        self.builder.get_object('powermax-label').set_text('Connecting')
                        self.builder.get_object('powermax-label').modify_fg(gtk.STATE_NORMAL, gtk.gdk.Color(blue = 1.0))
                        if self.connTimer:
                            if time.time() >= self.connTimer:
                                self.connTimer = 0
                                self.builder.get_object('powermax-enable').set_active(False)
                                self.pmx485Started = False
                                self.dialog_error('Communications Error', \
                                                  '\nCould not connect to Powermax\n' \
                                                  '\nCheck cables and connections\n' \
                                                  '\nCheck PM_PORT in .ini file\n')
                        else:
                            self.connTimer = time.time() + 5
                    self.pmx485Connected = False
                elif hal.get_value('pmx485.fault')and self.pmx485Connected:
                    faultRaw = '{:04.0f}'.format(hal.get_value('pmx485.fault'))
                    faultCode = '{}-{}-{}'.format(faultRaw[0], faultRaw[1:3], faultRaw[3])
                    if faultRaw in self.pmx485FaultName.keys():
                        if faultRaw == '0210' and hal.get_value('pmx485.current_max') > 110:
                            faultMsg = self.pmx485FaultName[faultRaw][1]
                        elif faultRaw == '0210':
                            faultMsg = self.pmx485FaultName[faultRaw][0]
                        else:
                            faultMsg = self.pmx485FaultName[faultRaw]
                        if faultRaw != self.fault:
                            self.fault = faultRaw
                            self.builder.get_object('powermax-label').set_text('Fault Code: {}'.format(faultCode))
                            self.builder.get_object('powermax-label').set_tooltip_text('Powermax error:\n\n{}'.format(faultMsg))
                            self.builder.get_object('powermax-label').modify_fg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 0.75))
                            self.dialog_error('Powermax Error', '\nPowermax fault code: {}\n\n{}'.format(faultCode, faultMsg))
                    else:
                        self.builder.get_object('powermax-label').set_text('Fault Code: {}'.format(faultRaw))
                        self.builder.get_object('powermax-label').set_tooltip_text('Powermax error:\n\n{}'.format(faultRaw))
                        self.builder.get_object('powermax-label').modify_fg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 0.75))
                        self.dialog_error('Powermax Error', '\nUnknown Powermax fault code: {}'.format(faultRaw))
                elif hal.get_value('pmx485.mode') or hal.get_value('pmx485.current') or hal.get_value('pmx485.pressure'):
                    self.fault = '0000'
                    self.builder.get_object('powermax-label').modify_fg(gtk.STATE_NORMAL, gtk.gdk.Color(green = 0.8))
                    if not self.builder.get_object('powermax-label').get_text() == 'Connected':
                        if hal.get_value('pmx485.pressure_max') > 10:
                            self.builder.get_object('gas-pressure-label').set_text('Gas Pressure (psi)')
                            self.builder.get_object('gas-pressure').set_digits(0)
                            self.builder.get_object('gas-pressure-adj').set_lower(-0.1)
                            self.builder.get_object('gas-pressure-adj').set_upper(150)
                            self.builder.get_object('gas-pressure-adj').set_step_increment(1)
                        else:
                            self.builder.get_object('gas-pressure-label').set_text('Gas Pressure (bar)')
                            self.builder.get_object('gas-pressure').set_digits(1)
                            self.builder.get_object('gas-pressure-adj').set_lower(-0.1)
                            self.builder.get_object('gas-pressure-adj').set_upper(10)
                            self.builder.get_object('gas-pressure-adj').set_step_increment(0.1)
                        toolTip = 'Powermax cutting current'
                        for widget in ['cut-amps','cut-amps-label']:
                                self.builder.get_object(widget).set_tooltip_text(toolTip)
                        self.on_reload_clicked(None)
                    if not self.builder.get_object('powermax-label').get_text() == 'Connected':
                        self.builder.get_object('powermax-label').set_tooltip_text('status of powermax communications')
                    self.builder.get_object('powermax-label').set_text('Connected')
                    if not self.pmx485Connected:
                        if hal.get_value('pmx485.current_min') > 0 and hal.get_value('pmx485.current_max') > 0:
                            self.builder.get_object('cut-amps').set_range(hal.get_value('pmx485.current_min'), hal.get_value('pmx485.current_max'))
                    self.pmx485Connected = True
            else:
                self.fault = '0000'
                self.pmx485Started = False
                self.pmx485Connected = False
                self.builder.get_object('powermax-label').set_text('pmx485 unloaded')
                self.builder.get_object('powermax-label').set_tooltip_text('status of powermax communications')
                self.builder.get_object('powermax-label').modify_fg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 1.0))
                self.builder.get_object('powermax-enable').set_active(False)
                self.dialog_error('Communications Error', '\nPowermax component unloaded:\n\nCheck cables and connections\n\nThen re-enable\n')
        return True

    # for powermax communications
    def pmx485_check(self):
        if self.i.find('PLASMAC', 'PM_PORT'):
            if not hal.component_exists('pmx485'):
                print('\n*** pmx485 component not loaded ***\n')
                raise SystemExit
            self.builder.get_object('powermax-enable').connect('toggled', self.pmx485_state_change)
            self.builder.get_object('cut-mode').connect('value_changed',self.pmx485_mode_changed)
            hal.new_sig('plasmac:powermax-current-set',hal.HAL_FLOAT)
            hal.connect('plasmac_run.cut-amps-f','plasmac:powermax-current-set')
            hal.connect('pmx485.current_set','plasmac:powermax-current-set')
            hal.new_sig('plasmac:powermax-pressure-set',hal.HAL_FLOAT)
            hal.connect('plasmac_run.gas-pressure-f','plasmac:powermax-pressure-set')
            hal.connect('pmx485.pressure_set','plasmac:powermax-pressure-set')
            self.pmx485_state_change(self.builder.get_object('powermax-enable'))
            self.builder.get_object('gas-pressure').connect('value-changed', self.pmx485_pressure_changed)
            self.pressure = self.builder.get_object('gas-pressure').get_value()
            self.connTimer = 0
        else:
            self.builder.get_object('gas-pressure').hide()
            self.builder.get_object('gas-pressure-label').hide()
            self.builder.get_object('cut-mode').hide()
            self.builder.get_object('cut-mode-label').hide()
            self.builder.get_object('powermax-frame').hide()

    # for powermax communications
    def pmx485_mode_changed(self, widget):
        self.builder.get_object('gas-pressure').set_value(0)
        hal.set_p('pmx485.mode_set', str(self.builder.get_object('cut-mode').get_value())) 

    # for powermax communications
    def pmx485_pressure_changed(self, widget):
        if self.pmx485Started:
            if self.builder.get_object('gas-pressure').get_value() < self.pressure:
                if self.builder.get_object('gas-pressure').get_value() < 0:
                    self.builder.get_object('gas-pressure').set_value(hal.get_value('pmx485.pressure_max'))
                elif self.builder.get_object('gas-pressure').get_value() < hal.get_value('pmx485.pressure_min'):
                    self.builder.get_object('gas-pressure').set_value(0)
            elif self.builder.get_object('gas-pressure').get_value() > self.pressure:
                if self.builder.get_object('gas-pressure').get_value() > hal.get_value('pmx485.pressure_max'):
                    self.builder.get_object('gas-pressure').set_value(0)
                elif self.builder.get_object('gas-pressure').get_value() < hal.get_value('pmx485.pressure_min'):
                    self.builder.get_object('gas-pressure').set_value(hal.get_value('pmx485.pressure_min'))
            self.pressure = self.builder.get_object('gas-pressure').get_value()

    # for powermax communications
    def pmx485_state_change(self, widget):
        if widget.get_active():
#            print('Starting Powermax communications')
            if not hal.component_exists('pmx485'):
                port = self.i.find('PLASMAC', 'PM_PORT')
                Popen('halcmd loadusr -Wn pmx485 ./pmx485.py {}'.format(port), stdout = PIPE, shell = True)
                timeout = time.time() + 3
                while 1:
                    time.sleep(0.1)
                    if time.time() > timeout:
                        self.builder.get_object('powermax-enable').set_active(False)
                        self.builder.get_object('powermax-label').set_text('')
                        self.builder.get_object('powermax-label').set_tooltip_text('status of powermax communications')
                        self.dialog_error('Communication Error', '\nTimeout while reconnecting\n\nCheck cables and connections\n\nThen re-enable\n')
                        return
                    if hal.component_exists('pmx485'):
#                        print('Powermax component reloaded') 
                        hal.connect('pmx485.current_set','plasmac:powermax-current-set')
                        hal.connect('pmx485.pressure_set','plasmac:powermax-pressure-set')
                        break
            if self.builder.get_object('cut-mode').get_value() == 0 or self.builder.get_object('cut-amps').get_value() == 0:
                self.dialog_error('Materials Error', '\nInvalid Cut Mode or Cut Amps\n\nCannot connect to Powermax\n')
                self.builder.get_object('powermax-enable').set_active(False)
                self.pmx485Started = False
                return
            else:
                self.pmx485Started = True
                hal.set_p('pmx485.mode_set', str(self.builder.get_object('cut-mode').get_value())) 
                hal.set_p('pmx485.enable', '1')
                self.connTimer = 0
        else:
#            if self.pmx485Started:
#                print('Stopping Powermax communications')
            self.pmx485Started = False
            self.pmx485Connected = False
            if hal.component_exists('pmx485'):
                hal.set_p('pmx485.enable', '0')
                self.builder.get_object('powermax-label').set_text('')
                self.builder.get_object('powermax-label').set_tooltip_text('status of powermax communications')
                self.builder.get_object('gas-pressure-adj').configure(0,0,0,1,0,0)
                toolTip = 'cutting current in amps\nindicator only, not used by plasmac.'
                for widget in ['cut-amps','cut-amps-label']:
                    self.builder.get_object(widget).set_tooltip_text(toolTip)

    # for powermax communications
    pmx485FaultName = {
                '0110': 'Remote controller mode invalid',
                '0111': 'Remote controller current invalid',
                '0112': 'Remote controller pressure invalid',
                '0120': 'Low input gas pressure',
                '0121': 'Output gas pressure low',
                '0122': 'Output gas pressure high',
                '0123': 'Output gas pressure unstable',
                '0130': 'AC input power unstable',
                '0199': 'Power board hardware protection',
                '0200': 'Low gas pressure',
                '0210': ('Gas flow lost while cutting', 'Excessive arc voltage'),
                '0220': 'No gas input',
                '0300': 'Torch stuck open',
                '0301': 'Torch stuck closed',
                '0320': 'End of consumable life',
                '0400': 'PFC/Boost IGBT module under temperature',
                '0401': 'PFC/Boost IGBT module over temperature',
                '0402': 'Inverter IGBT module under temperature',
                '0403': 'Inverter IGBT module over temperature',
                '0500': 'Retaining cap off',
                '0510': 'Start/trigger signal on at power up',
                '0520': 'Torch not connected',
                '0600': 'AC input voltage phase loss',
                '0601': 'AC input voltage too low',
                '0602': 'AC input voltage too high',
                '0610': 'AC input unstable',
                '0980': 'Internal communication failure',
                '0990': 'System hardware fault',
                '1000': 'Digital signal processor fault',
                '1100': 'A/D converter fault',
                '1200': 'I/O fault',
                '2000': 'A/D converter value out of range',
                '2010': 'Auxiliary switch disconnected',
                '2100': 'Inverter module temp sensor open',
                '2101': 'Inverter module temp sensor shorted',
                '2110': 'Pressure sensor is open',
                '2111': 'Pressure sensor is shorted',
                '2200': 'DSP does not recognize the torch',
                '3000': 'Bus voltage fault',
                '3100': 'Fan speed fault',
                '3101': 'Fan fault',
                '3110': 'PFC module temperature sensor open',
                '3111': 'PFC module temperature sensor shorted',
                '3112': 'PFC module temperature sensor circuit fault',
                '3200': 'Fill valve',
                '3201': 'Dump valve',
                '3201': 'Valve ID',
                '3203': 'Electronic regulator is disconnected',
                '3410': 'Drive fault',
                '3420': '5 or 24 VDC fault',
                '3421': '18 VDC fault',
                '3430': 'Inverter capacitors unbalanced',
                '3441': 'PFC over current',
                '3511': 'Inverter saturation fault',
                '3520': 'Inverter shoot-through fault',
                '3600': 'Power board fault',
                '3700': 'Internal serial communications fault',
                }

    def idle_changed(self, halpin):
        if not halpin.get():
            for key in sorted(self.configDict.iterkeys()):
                if isinstance(self.builder.get_object(key), gladevcp.hal_widgets.HAL_SpinButton):
                    self.builder.get_object(key).update()

    def upgrade_check(self):
        # the latest version that configurator is required for *****************
        # this may or may not be the current version ***************************
        latestMajorUpgrade = 0.144
        lastMajorUpgrade = float(self.i.find('PLASMAC', 'LAST_MAJOR_UPGRADE') or self.i.find('PLASMAC', 'LAST_UPGRADE') or 0.0)
        manualUpgrade = int(self.i.find('PLASMAC', 'MANUAL_UPGRADE') or 0)
        if lastMajorUpgrade < latestMajorUpgrade and manualUpgrade != 1:
            iniFile = os.environ['INI_FILE_NAME']
            iniPath = os.path.dirname(iniFile)
            try:
                basePath = os.path.realpath(os.path.dirname(os.readlink('{}/plasmac'.format(iniPath))))
            except:
                try:
                    basePath = os.path.realpath(os.path.dirname(os.readlink('{}/M190'.format(iniPath))))
                except:
                    print('\nlink to plasmac source files cannot be found\n')
                    sys.exit(0)
            cmd = '{}/configurator.py'.format(basePath)
            os.execv(cmd,[cmd, 'upgrade', iniFile])
            sys.exit(0)

    def __init__(self, halcomp,builder,useropts):
        self.W = gtk.Window()
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.upgrade_check()
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.cutTypePin = hal_glib.GPin(halcomp.newpin('cut-type', hal.HAL_S32, hal.HAL_IN))
        self.materialNumberPin = hal_glib.GPin(halcomp.newpin('material-change-number', hal.HAL_S32, hal.HAL_IN))
        self.materialChangePin = hal_glib.GPin(halcomp.newpin('material-change', hal.HAL_S32, hal.HAL_IN))
        self.materialTimeoutPin = hal_glib.GPin(halcomp.newpin('material-change-timeout', hal.HAL_BIT, hal.HAL_IN))
        self.firstMaterialPin = hal_glib.GPin(halcomp.newpin('first-material', hal.HAL_S32, hal.HAL_IN))
        self.materialReloadPin = hal_glib.GPin(halcomp.newpin('material-reload', hal.HAL_BIT, hal.HAL_IN))
        self.tempMaterialPin = hal_glib.GPin(halcomp.newpin('temp-material', hal.HAL_BIT, hal.HAL_IN))
        self.thcEnablePin = hal_glib.GPin(halcomp.newpin('thc-enable-out', hal.HAL_BIT, hal.HAL_OUT))
        self.materialNumberPin.connect('value-changed', self.material_change_number_changed)
        self.materialChangePin.connect('value-changed', self.material_change_changed)
        self.materialTimeoutPin.connect('value-changed', self.material_change_timeout)
        self.firstMaterialPin.connect('value-changed', self.first_material_changed)
        self.materialReloadPin.connect('value-changed', self.on_material_reload_pin)
        self.tempMaterialPin.connect('value-changed', self.on_temp_material_pin)
        self.idlePin = hal_glib.GPin(halcomp.newpin('program-is-idle', hal.HAL_BIT, hal.HAL_IN))
        hal.connect('plasmac_run.program-is-idle', 'plasmac:program-is-idle') 
        self.idlePin.connect('value-changed', self.idle_changed)
        self.previewPin = hal_glib.GPin(halcomp.newpin('preview-tab', hal.HAL_BIT, hal.HAL_IN))
        self.thcFeedRate = (float(self.i.find('AXIS_Z', 'MAX_VELOCITY')) * \
                            float(self.i.find('AXIS_Z', 'OFFSET_AV_RATIO'))) * 60
        hal.set_p('plasmac.thc-feed-rate','{}'.format(self.thcFeedRate))
        self.configFile = self.i.find('EMC', 'MACHINE').lower() + '_run.cfg'
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.materialFile = self.i.find('EMC', 'MACHINE').lower() + '_material.cfg'
        self.tmpMaterialFile = self.materialFile.replace('.cfg','.tmp')
        self.materialFileDict = {}
        self.materialDict = {}
        self.configDict = {}
        self.materialNumList = []
        hal.set_p('plasmac.mode','{}'.format(int(self.i.find('PLASMAC','MODE') or '0')))
        self.oldMode = 9
        self.materialUpdate = False
        self.autoChange = False
        self.fault = '0000'
        self.configure_widgets()
        self.load_settings()
        self.check_material_file()
        self.get_material()
        self.set_theme()
        self.pmx485Started = False
        self.pmx485Connected = False
        self.pmx485_check()
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
