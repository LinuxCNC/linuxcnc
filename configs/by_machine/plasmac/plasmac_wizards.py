#!/usr/bin/env python

'''
plasmac_wizards.py

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

import gi
from gi.repository import Gtk as gtk
from gi.repository import GObject as gobject
import os
import sys
import linuxcnc
import shutil
import hal
import time
from subprocess import Popen, PIPE

sys.path.append('./wizards')
import w_main
import w_cut_recovery

class wizards:

    def __init__(self, halcomp,builder,useropts):
        self.W = gtk.Window()
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.s = linuxcnc.stat()
        self.c = linuxcnc.command()
        self.builder = builder
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = self.i.find('EMC', 'MACHINE').lower() + '_wizards.cfg'
        self.cutRecovering = False
        self.check_settings()
        self.set_theme()
        self.button_setup()
        self.builder.get_object('button10').connect('realize', self.set_style)
        self.initialized = True
        self.filter = './plasmac/plasmac_gcode.py'
        self.wizardButton = self.builder.get_object('wizard-b')
        self.buttonBG = 'none'
        gobject.timeout_add(100, self.periodic)

    def set_theme(self):
        theme = gtk.settings_get_default().get_property('gtk-theme-name')
        if os.path.exists(self.prefFile):
            try:
                with open(self.prefFile, 'r') as f_in:
                    for line in f_in:
                        if 'gtk_theme' in line and not 'Follow System Theme' in line:
                            (item, theme) = line.strip().replace(" ", "").split('=')
            except:
                self.dialog_error('Preferences file, {} is invalid ***'.format(self.prefFile))
                print('*** preferences file, {} is invalid ***'.format(self.prefFile))
        else:
            theme = self.i.find('PLASMAC', 'THEME') or gtk.settings_get_default().get_property('gtk-theme-name')
            font = self.i.find('PLASMAC', 'FONT') or gtk.settings_get_default().get_property('gtk-font-name')
            gtk.settings_get_default().set_property('gtk-font-name', font)
        gtk.settings_get_default().set_property('gtk-theme-name', theme)

    def check_settings(self):
        if not os.path.exists(self.configFile):
            try:
                with open(self.configFile, 'w') as f_out:
                    f_out.write('#plasmac wizards configuration file, format is:\n#name = value\n\n')
                    ambles = 'G40 G49 G80 G90 G92.1 G94 G97'
                    if self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch':
                        f_out.write('preamble=G20 G64P0.004 {}\n'.format(ambles))
                        f_out.write('postamble=G20 G64P0.004 {}\n'.format(ambles))
                        f_out.write('origin=False\n')
                        f_out.write('lead-in=0.16\n')
                        f_out.write('lead-out=0\n')
                        f_out.write('hole-diameter=1.26\n')
                    else:
                        f_out.write('preamble=G21 G64P0.1 {}\n'.format(ambles))
                        f_out.write('postamble=G21 G64P0.1 {}\n'.format(ambles))
                        f_out.write('origin=False\n')
                        f_out.write('lead-in=4.0\n')
                        f_out.write('lead-out=0\n')
                        f_out.write('hole-diameter=32\n')
                    f_out.write('hole-speed=60\n')
                    f_out.write('window-width=890\n')
                    f_out.write('window-height=582\n')
                    f_out.write('grid-size=0\n')
                    f_out.write('font-size=9\n')
            except:
                self.dialog_error('Error opening {}'.format(self.configFile))

    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
                               gtk.DialogFlags.DESTROY_WITH_PARENT,
                               gtk.MESSAGE_ERROR, 
                               gtk.BUTTONS_CLOSE,
                               error)
        md.set_keep_above(True)
        md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        md.run()
        md.destroy()

    def on_wizard_b_clicked(self, widget):
        self.wizardButton.set_sensitive(False)
        reload(w_main)
        main_wiz = w_main.main_wiz()
        error = main_wiz.main_show(self)
        if error:
            self.dialog_error('Error in conversational dialog')

    def button_setup(self):
        self.iniButtonName = ['Names','','','','','','','','','']
        self.iniButtonCode = ['Codes','','','','','','','','','']
        self.probePressed = False
        self.probeTimer = 0
        self.probeButton = ''
        self.cutType = 0
        self.inFile = ''
        self.cutButton = ''
        for button in range(10,20):
            bname = self.i.find('PLASMAC', 'BUTTON_' + str(button) + '_NAME') or '0'
            self.iniButtonName.append(bname)
            code = self.i.find('PLASMAC', 'BUTTON_' + str(button) + '_CODE') or ''
            self.iniButtonCode.append(code)
            pic = self.i.find('PLASMAC', 'BUTTON_' + str(button) + '_IMAGE') or ''
            if bname != '0':
                bname = bname.split('\\')
                blabel = bname[0]
                if len(bname) > 1:
                    for name in range(1, len(bname)):
                        blabel += '\n{}'.format(bname[name])
                self.builder.get_object('button' + str(button)).set_label(blabel)
                self.builder.get_object('button' + str(button)).children()[0].set_justify(gtk.JUSTIFY_CENTER)
            if code == 'cut-type':
                self.cutButton = 'button{}'.format(button)
            if 'change-consumables' in code:
                self.ccParm = self.i.find('PLASMAC','BUTTON_' + str(button) + '_CODE').replace('change-consumables','').replace(' ','').lower() or None
            try:
                if pic:
                    pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                            filename='{}/{}'.format(os.environ['CONFIG_DIR'], pic), 
                            width=60, 
                            height=60)
                    image = gtk.Image()
                    image.set_from_pixbuf(pixbuf)
                    self.builder.get_object('button' + str(button)).set_label('')
                    self.builder.get_object('button' + str(button)).set_image(image)
            except:
                print('Could not load image for custom user button #{}'.format(button))

    def on_button_pressed(self, button):
        bNum = int(button.get_name().split('button')[1])
        commands = self.iniButtonCode[bNum]
        if not commands: return
        if 'cut-recovery' in commands.lower() and hal.get_value('halui.program.is-paused') and \
           not hal.get_value('plasmac.cut-recovery') and not hal.get_value('plasmac.cut-recovering'):
            cut_wiz = w_cut_recovery.recovery()
            error = cut_wiz.create_widgets()
            if error:
                self.dialog_error('Error in cut recovery dialog')
        elif 'change-consumables' in commands.lower():
            self.consumable_change_setup()
            if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
                hal.set_p('plasmac.consumable-change', '0')
                hal.set_p('plasmac.x-offset', '0')
                hal.set_p('plasmac.y-offset', '0')
            else:
                if self.ccF == 'None' or self.ccF < 1:
                    self.dialog_error('Invalid feed rate for consumable change\n\nCheck .ini file settings\n\nBUTTON_{}_CODE'.format(str(button)))
                    return
                else:
                    hal.set_p('plasmac.xy-feed-rate', str(float(self.ccF)))
                if self.ccX == 'None':
                    self.ccX = self.s.position[0]
                if self.ccX < round(float(self.i.find('AXIS_X', 'MIN_LIMIT')), 6) + (10 * hal.get_value('halui.machine.units-per-mm')):
                    self.ccX = round(float(self.i.find('AXIS_X', 'MIN_LIMIT')), 6) + (10 * hal.get_value('halui.machine.units-per-mm'))
                elif self.ccX > round(float(self.i.find('AXIS_X', 'MAX_LIMIT')), 6) - (10 * hal.get_value('halui.machine.units-per-mm')):
                    self.ccX = round(float(self.i.find('AXIS_X', 'MAX_LIMIT')), 6) - (10 * hal.get_value('halui.machine.units-per-mm'))
                if self.ccY == 'None':
                    self.ccY = self.s.position[1]
                if self.ccY < round(float(self.i.find('AXIS_Y', 'MIN_LIMIT')), 6) + (10 * hal.get_value('halui.machine.units-per-mm')):
                    self.ccY = round(float(self.i.find('AXIS_Y', 'MIN_LIMIT')), 6) + (10 * hal.get_value('halui.machine.units-per-mm'))
                elif self.ccY > round(float(self.i.find('AXIS_Y', 'MAX_LIMIT')), 6) - (10 * hal.get_value('halui.machine.units-per-mm')):
                    self.ccY = round(float(self.i.find('AXIS_Y', 'MAX_LIMIT')), 6) - (10 * hal.get_value('halui.machine.units-per-mm'))
                hal.set_p('plasmac.x-offset', '{:.0f}'.format((self.ccX - self.s.position[0]) / hal.get_value('plasmac.offset-scale')))
                hal.set_p('plasmac.y-offset', '{:.0f}'.format((self.ccY - self.s.position[1]) / hal.get_value('plasmac.offset-scale')))
                hal.set_p('plasmac.consumable-change', '1')
        elif commands.lower() == 'ohmic-test':
            hal.set_p('plasmac.ohmic-test','1')
        elif 'probe-test' in commands.lower():
            if not self.probeTimer and not hal.get_value('plasmac.z-offset-counts'):
                self.probePressed = True
                self.probeButton = button
                if commands.lower().replace('probe-test','').strip():
                    self.probeStart = time.time()
                    self.probeTimer = float(commands.lower().replace('probe-test','').strip())
                    hal.set_p('plasmac.probe-test','1')
                    self.probeText = self.probeButton.get_label()
                    self.probeButton.set_label(str(int(self.probeTimer)))
                    self.probeButton.set_style(self.buttonRed)
        elif 'cut-type' in commands.lower() and not hal.get_value('halui.program.is-running') and self.s.file:
            self.cutType ^= 1
            if not 'PlaSmaC' in self.s.file:
                self.inFile = self.s.file
            self.inPath = os.path.realpath(os.path.dirname(self.inFile))
            self.inBase = os.path.basename(self.inFile)
            if self.cutType:
                hal.set_p('plasmac_run.cut-type','1')
                self.outFile = '{}/PlaSmaC1_{}'.format(self.inPath,self.inBase)
                self.builder.get_object(self.cutButton).set_style(self.buttonOrange)
                self.builder.get_object(self.cutButton).set_label('Pierce Only')
            else:
                hal.set_p('plasmac_run.cut-type','0')
                self.outFile = '{}/PlaSmaC0_{}'.format(self.inPath,self.inBase)
                self.builder.get_object(self.cutButton).set_style(self.buttonPlain)
                self.builder.get_object(self.cutButton).set_label('Pierce & Cut')
            if self.gui == 'axis':
                Popen('axis-remote -r', stdout = PIPE, shell = True)
            else:
                outBuf = open(self.outFile, 'w')
                filter = Popen(['sh', '-c', '%s \'%s\'' % (self.filter, self.inFile)], stdin=PIPE, stdout=outBuf, stderr=PIPE)
                filter.stdin.close()
                stderr_text = []
                try:
                    while filter.poll() is None:
                        pass
                finally:
                    outBuf.close()
                self.c.program_open(self.outFile)
                hal.set_p('plasmac_run.cut-type','0')
        elif 'load' in commands.lower():
            path = self.i.find('DISPLAY', 'PROGRAM_PREFIX') or os.environ['LINUXCNC_NCFILES_DIR']
            lFile = commands.split('load')[1].strip()
            if self.gui == 'axis':
                Popen('axis-remote {}/{}'.format(path, lFile), stdout = PIPE, shell = True)
            elif self.gui == 'gmoccapy':
                self.c.program_open('{}/{}'.format(path, lFile))
        elif 'toggle-halpin' in commands.lower() and hal.get_value('halui.program.is-idle'):
            halpin = commands.lower().split('toggle-halpin')[1].strip()
            pinstate = hal.get_value(halpin)
            hal.set_p(halpin, str(not pinstate))
            if pinstate:
                button.set_style(self.buttonPlain)
            else:
                button.set_style(self.buttonGreen)
        else:
            for command in commands.split('\\'):
                if command.strip()[0] == '%':
                    command = command.strip().strip('%') + '&'
                    msg = Popen(command,stdout=PIPE,stderr=PIPE, shell=True)
                    print(msg.communicate()[0])
                else:
                    if '{' in command:
                        newCommand = subCommand = ''
                        for char in command:
                            if char == '{':
                                subCommand = ':'
                            elif char == '}':
                                f1, f2 = subCommand.replace(':',"").split()
                                newCommand += self.i.find(f1,f2)
                                subCommand = ''
                            elif subCommand.startswith(':'):
                                subCommand += char
                            else:
                                newCommand += char
                        command = newCommand
                    self.s.poll()
                    if not self.s.estop and self.s.enabled and self.s.homed and (self.s.interp_state == linuxcnc.INTERP_IDLE):
                        mode = self.s.task_mode
                        if mode != linuxcnc.MODE_MDI:
                            mode = self.s.task_mode
                            self.c.mode(linuxcnc.MODE_MDI)
                            self.c.wait_complete()
                        self.c.mdi(command)
                        self.s.poll()
                        while self.s.interp_state != linuxcnc.INTERP_IDLE:
                            self.s.poll()
                        self.c.mode(mode)
                        self.c.wait_complete()

    def on_button_released(self, button):
        bNum = int(button.get_name().split('button')[1])
        commands = self.iniButtonCode[bNum]
        if not commands: return
        if 'ohmic-test' in commands.lower():
            hal.set_p('plasmac.ohmic-test','0')
        elif 'probe-test' in commands.lower():
            self.probePressed = False
            if not self.probeTimer and button == self.probeButton:
                hal.set_p('plasmac.probe-test','0')
                self.probeButton.set_label(self.probeText)
                self.probeButton.set_style(self.buttonPlain)
                self.probeButton = ''

    def consumable_change_setup(self):
        self.ccX = self.ccY = self.ccF = 'None'
        X = Y = F = ''
        ccAxis = [X, Y, F]
        ccName = ['x', 'y', 'f']
        for loop in range(3):
            count = 0
            if ccName[loop] in self.ccParm:
                while 1:
                    if not self.ccParm[count]: break
                    if self.ccParm[count] == ccName[loop]:
                        count += 1
                        break
                    count += 1
                while 1:
                    if count == len(self.ccParm): break
                    if self.ccParm[count].isdigit() or self.ccParm[count] in '.-':
                        ccAxis[loop] += self.ccParm[count]
                    else:
                        break
                    count += 1
                if ccName[loop] == 'x' and ccAxis[loop]:
                    self.ccX = float(ccAxis[loop])
                elif ccName[loop] == 'y' and ccAxis[loop]:
                    self.ccY = float(ccAxis[loop])
                elif ccName[loop] == 'f' and ccAxis[loop]:
                    self.ccF = float(ccAxis[loop])

    def set_style(self,button):
        self.buttonPlain = self.builder.get_object('button10').get_style().copy()
        self.buttonOrange = self.builder.get_object('button10').get_style().copy()
        self.buttonOrange.bg[gtk.STATE_NORMAL] = gtk.gdk.color_parse('orange')
        self.buttonOrange.bg[gtk.STATE_PRELIGHT] = gtk.gdk.color_parse('dark orange')
        self.buttonRed = self.builder.get_object('button10').get_style().copy()
        self.buttonRed.bg[gtk.STATE_NORMAL] = gtk.gdk.color_parse('red')
        self.buttonRed.bg[gtk.STATE_PRELIGHT] = gtk.gdk.color_parse('red')
        self.buttonGreen = self.builder.get_object('button10').get_style().copy()
        self.buttonGreen.bg[gtk.STATE_NORMAL] = gtk.gdk.color_parse('green')
        self.buttonGreen.bg[gtk.STATE_PRELIGHT] = gtk.gdk.color_parse('green')

    def periodic(self):
        self.s.poll()
        isIdleHomed = True
        isIdleOn = True
        if hal.component_exists('halui'):
            if hal.get_value('plasmac:program-is-idle'):
                hal.set_p('plasmac.cut-recovery', '0')
            if hal.get_value('halui.machine.is-on'):
                if hal.get_value('plasmac.arc-ok-out'):
                    isIdleOn = False
                for joint in range(0,int(self.i.find('KINS','JOINTS'))):
                        if not self.s.homed[joint]:
                            isIdleHomed = False
                            break
            else:
                isIdleHomed = False
                isIdleOn = False 
            for n in range(10,20):
                if 'load' in self.iniButtonCode[n]:
                    pass
                elif 'change-consumables' in self.iniButtonCode[n]:
                    if hal.get_value('halui.program.is-paused') and \
                       hal.get_value('plasmac.stop-type-out') > 1 and \
                       not hal.get_value('plasmac.cut-recovering'):
                        self.builder.get_object('button' + str(n)).set_sensitive(True)
                    else:
                        self.builder.get_object('button' + str(n)).set_sensitive(False)
                elif 'cut-recovery' in self.iniButtonCode[n]:
                    if hal.get_value('halui.program.is-paused') and hal.get_value('plasmac.motion-type') > 1 and \
                       not hal.get_value('plasmac.consumable-changing'):
                        self.builder.get_object('button' + str(n)).set_sensitive(True)
                    else:
                        self.builder.get_object('button' + str(n)).set_sensitive(False)
                elif 'ohmic-test' in self.iniButtonCode[n]:
                    if isIdleOn or hal.get_value('halui.program.is-paused'):
                        self.builder.get_object('button' + str(n)).set_sensitive(True)
                    else:
                        self.builder.get_object('button' + str(n)).set_sensitive(False)
                elif not 'cut-type' in self.iniButtonCode[n] and not self.iniButtonCode[n].startswith('%'):
                    if isIdleHomed:
                        self.builder.get_object('button' + str(n)).set_sensitive(True)
                    else:
                        self.builder.get_object('button' + str(n)).set_sensitive(False)
            # decrement probe timer if active
            if self.probeTimer:
                if hal.get_value('plasmac.probe-test-error') and not self.probePressed:
                    self.probeTimer = 0
                elif time.time() >= self.probeStart + 1:
                    self.probeStart += 1
                    self.probeTimer -= 1
                    self.probeButton.set_label(str(int(self.probeTimer)))
                    self.probeButton.set_style(self.buttonRed)
                if not self.probeTimer and not self.probePressed:
                    hal.set_p('plasmac.probe-test','0')
                    self.probeButton.set_label(self.probeText)
                    self.probeButton.set_style(self.buttonPlain)
            if self.gui == 'gmoccapy':
                if self.inFile and self.inFile != self.s.file:
                    if not 'PlaSmaC' in self.s.file or 'PlaSmaC0' in self.s.file:
                        self.builder.get_object(self.cutButton).set_style(self.buttonPlain)
                        self.builder.get_object(self.cutButton).set_label('Pierce & Cut')
                        self.cutType = 0
                    elif 'PlaSmaC1' in self.s.file:
                        self.builder.get_object(self.cutButton).set_style(self.buttonOrange)
                        self.builder.get_object(self.cutButton).set_label('Pierce Only')
                        self.cutType = 1
            if (hal.get_value('axis.x.eoffset') or hal.get_value('axis.y.eoffset')) and not hal.get_value('halui.program.is-paused'):
                hal.set_p('plasmac.consumable-change', '0')
                hal.set_p('plasmac.x-offset', '0')
                hal.set_p('plasmac.y-offset', '0')
                hal.set_p('plasmac.xy-feed-rate', '0')
        return True

def get_handlers(halcomp,builder,useropts):
    return [wizards(halcomp,builder,useropts)]
