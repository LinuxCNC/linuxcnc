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

import os
import gtk
import linuxcnc
import gobject
import hal
import gladevcp
from subprocess import Popen,PIPE
import time

class HandlerClass:

    def on_button1_pressed(self,button):
        self.user_button_pressed(button.get_name(),self.iniButtonCode[1])

    def on_button1_released(self,button):
        self.user_button_released(button.get_name(),self.iniButtonCode[1])

    def on_button2_pressed(self,button):
        self.user_button_pressed(button.get_name(),self.iniButtonCode[2])

    def on_button2_released(self,button):
        self.user_button_released(button.get_name(),self.iniButtonCode[2])

    def on_button3_pressed(self,button):
        self.user_button_pressed(button.get_name(),self.iniButtonCode[3])

    def on_button3_released(self,button):
        self.user_button_released(button.get_name(),self.iniButtonCode[3])

    def on_button4_pressed(self,button):
        self.user_button_pressed(button.get_name(),self.iniButtonCode[4])

    def on_button4_released(self,button):
        self.user_button_released(button.get_name(),self.iniButtonCode[4])

    def user_button_pressed(self, button, commands):
        if not commands: return
        if commands.lower() == 'ohmic-test':
            hal.set_p('plasmac.ohmic-test','1')
        elif 'probe-test' in commands.lower():
            self.probePressed = True
            self.probeButton = button
            if commands.lower().replace('probe-test','').strip():
                self.probeTimer = float(commands.lower().replace('probe-test','').strip()) + time.time()
            hal.set_p('plasmac.probe-test','1')
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
            filter = subprocess.Popen(['sh', '-c', '%s \'%s\'' % ('./plasmac_gcode.py', self.inFile)],
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
        self.probePressed = False
        if not commands: return
        if commands.lower() == 'ohmic-test':
            hal.set_p('plasmac.ohmic-test','0')
        elif 'probe-test' in commands.lower():
            if not self.probeTimer and button == self.probeButton:
                hal.set_p('plasmac.probe-test','0')
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
            if 'ohmic-test' in self.iniButtonCode[n]:
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
            if time.time() >= self.probeTimer:
                self.probeTimer = 0
                if not self.probePressed:
                    hal.set_p('plasmac.probe-test','0')
        if self.inFile and self.inFile != self.s.file:
            if not 'PlaSmaC' in self.s.file or 'PlaSmaC0' in self.s.file:
                self.builder.get_object(self.cutButton).set_style(self.buttonPlain)
                self.builder.get_object(self.cutButton).set_label('Pierce & Cut')
                self.cutType = 0
            elif 'PlaSmaC1' in self.s.file:
                self.builder.get_object(self.cutButton).set_style(self.buttonOrange)
                self.builder.get_object(self.cutButton).set_label('Pierce Only')
                self.cutType = 1
        return True

    def __init__(self, halcomp,builder,useropts):
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
        self.set_theme()
        self.builder.get_object('button1').connect('realize', self.set_style)
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
