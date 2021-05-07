#!/usr/bin/env python

'''
plasmac_buttons.py
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

import gi
from gi.repository import Gtk as gtk
from gi.repository import GObject as gobject
import os
import linuxcnc
import hal
import gladevcp
from subprocess import Popen,PIPE
import time

class HandlerClass:

    def on_button1_pressed(self,button):
        self.user_button_pressed(button,self.iniButtonCode[1])

    def on_button1_released(self,button):
        self.user_button_released(button,self.iniButtonCode[1])

    def on_button2_pressed(self,button):
        self.user_button_pressed(button,self.iniButtonCode[2])

    def on_button2_released(self,button):
        self.user_button_released(button,self.iniButtonCode[2])

    def on_button3_pressed(self,button):
        self.user_button_pressed(button,self.iniButtonCode[3])

    def on_button3_released(self,button):
        self.user_button_released(button,self.iniButtonCode[3])

    def on_button4_pressed(self,button):
        self.user_button_pressed(button,self.iniButtonCode[4])

    def on_button4_released(self,button):
        self.user_button_released(button,self.iniButtonCode[4])

    def user_button_pressed(self, button, commands):
        if not commands: return
        if 'change-consumables' in commands.lower():
            self.consumable_change_setup()
            if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
                hal.set_p('plasmac.consumable-change', '0')
                hal.set_p('plasmac.x-offset', '0')
                hal.set_p('plasmac.y-offset', '0')
                hal.set_p('plasmac.xy-feed-rate', '0')
            else:
                if self.ccF == 'None' or self.ccF < 1:
                    self.dialog_error('Invalid feed rate for consumable change\n\nCheck .ini file settings\n')
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
            else:
                hal.set_p('plasmac_run.cut-type','0')
                self.outFile = '{}/PlaSmaC0_{}'.format(self.inPath,self.inBase)
            import subprocess
            outBuf = open(self.outFile, 'w')
            filter = subprocess.Popen(['sh', '-c', '%s \'%s\'' % ('./plasmac/plasmac_gcode.py', self.inFile)],
                                  stdin=subprocess.PIPE,
                                  stdout=outBuf,
                                  stderr=subprocess.PIPE)
            filter.stdin.close()
            stderr_text = []
            try:
                while filter.poll() is None:
                    pass
            finally:
                outBuf.close()
            self.c.program_open(self.outFile)
            hal.set_p('plasmac_run.cut-type','0')
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
                    Popen(command,stdout=PIPE,stderr=PIPE, shell=True)
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

    def user_button_released(self, button, commands):
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
        gtk.settings_get_default().set_property('gtk-theme-name', theme)

    def set_style(self,button):
        self.buttonPlain = self.builder.get_object('button1').get_style().copy()
        self.buttonOrange = self.builder.get_object('button1').get_style().copy()
        self.buttonOrange.bg[gtk.STATE_NORMAL] = gtk.gdk.color_parse('orange')
        self.buttonOrange.bg[gtk.STATE_PRELIGHT] = gtk.gdk.color_parse('dark orange')
        self.buttonRed = self.builder.get_object('button1').get_style().copy()
        self.buttonRed.bg[gtk.STATE_NORMAL] = gtk.gdk.color_parse('red')
        self.buttonRed.bg[gtk.STATE_PRELIGHT] = gtk.gdk.color_parse('red')
        self.buttonGreen = self.builder.get_object('button1').get_style().copy()
        self.buttonGreen.bg[gtk.STATE_NORMAL] = gtk.gdk.color_parse('green')
        self.buttonGreen.bg[gtk.STATE_PRELIGHT] = gtk.gdk.color_parse('green')

    def periodic(self):
        self.s.poll()
        isIdleHomed = True
        isIdleOn = True
        if hal.get_value('halui.program.is-idle') and hal.get_value('halui.machine.is-on'):
            if hal.get_value('plasmac.arc-ok-out'):
                isIdleOn = False
            for joint in range(0,int(self.i.find('KINS','JOINTS'))):
                    if not self.s.homed[joint]:
                        isIdleHomed = False
                        break
        else:
            isIdleHomed = False
            isIdleOn = False 
        for n in range(1,5):
            if 'change-consumables' in self.iniButtonCode[n]:
                if hal.get_value('halui.program.is-paused') and \
                   hal.get_value('plasmac.stop-type-out') > 1 and not \
                   hal.get_value('plasmac.cut-recovering'):
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

    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
            gtk.DialogFlags.DESTROY_WITH_PARENT, gtk.MESSAGE_ERROR, 
            gtk.BUTTONS_CLOSE, error)
        md.run()
        md.destroy()

    def __init__(self, halcomp,builder,useropts):
        self.W = gtk.Window()
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.s = linuxcnc.stat()
        self.c = linuxcnc.command()
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.iniButtonName = ['Names']
        self.iniButtonCode = ['Codes']
        self.probePressed = False
        self.probeTimer = 0
        self.probeButton = ''
        self.cutType = 0
        self.inFile = ''
        self.cutButton = ''
        for button in range(1,5):
            bname = self.i.find('PLASMAC', 'BUTTON_' + str(button) + '_NAME') or '0'
            self.iniButtonName.append(bname)
            code = self.i.find('PLASMAC', 'BUTTON_' + str(button) + '_CODE')
            self.iniButtonCode.append(code)
            if code == 'cut-type':
                self.cutButton = 'button{}'.format(button)
            if bname != '0':
                bname = bname.split('\\')
                if len(bname) > 1:
                    blabel = bname[0] + '\n' + bname[1]
                else:
                    blabel = bname[0]
                self.builder.get_object('button' + str(button)).set_label(blabel)
                self.builder.get_object('button' + str(button)).children()[0].set_justify(gtk.JUSTIFY_CENTER)
            if 'change-consumables' in code:
                self.ccParm = self.i.find('PLASMAC','BUTTON_' + str(button) + '_CODE').replace('change-consumables','').replace(' ','').lower() or None
        self.set_theme()
        self.builder.get_object('button1').connect('realize', self.set_style)
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
