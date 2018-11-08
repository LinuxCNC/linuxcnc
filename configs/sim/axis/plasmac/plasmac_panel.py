#!/usr/bin/env python

'''
plasmac_panel.py
Copyright (C) 2018  Phillip A Carter

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
import subprocess as sp
import gtk
import linuxcnc
import gobject
import hal
from   gladevcp.persistence import IniFile,widget_defaults,set_debug,select_widgets

debug = 0

class Global:
    materialList = []
    thcFeedRate = 0
    oldMode = 9

class linuxcncInterface(object):
    def send_command(self,command, wait=True):
        if self.s.interp_state == linuxcnc.INTERP_IDLE:
            if self.s.task_mode != linuxcnc.MODE_MDI:
                self.c.mode(linuxcnc.MODE_MDI)
                self.c.wait_complete()
            self.c.mdi(command)
            if wait:
                self.c.wait_complete()

    def __init__(self):
        self.linuxcncIniFile = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.stat = linuxcnc.stat();
        self.comd = linuxcnc.command()

class HandlerClass:
    def on_save_clicked(self,widget,data=None):
        self.ini.save_state(self)
        Global.materialList[0][1] = self.builder.get_object('pierceHeightAdj').get_value()
        Global.materialList[0][2] = self.builder.get_object('pierceDelayAdj').get_value()
        Global.materialList[0][3] = self.builder.get_object('puddleJumpDelayAdj').get_value()
        Global.materialList[0][4] = self.builder.get_object('cutHeightAdj').get_value()
        Global.materialList[0][5] = self.builder.get_object('cutFeedRateAdj').get_value()
        Global.materialList[0][6] = self.builder.get_object('cutAmpsAdj').get_value()
        Global.materialList[0][7] = self.builder.get_object('cutVoltsAdj').get_value()
        self.builder.get_object('material').set_active(0)

    def on_reload_clicked(self,widget,data=None):
        self.ini.restore_state(self)
        self.builder.get_object('material').set_active(0)

    def on_material_changed(self,widget,data=None):
        material = self.builder.get_object('material').get_active()
        self.builder.get_object('pierceHeightAdj').set_value(Global.materialList[material][1])
        self.builder.get_object('pierceDelayAdj').set_value(Global.materialList[material][2])
        self.builder.get_object('puddleJumpDelayAdj').set_value(Global.materialList[material][3])
        self.builder.get_object('cutHeightAdj').set_value(Global.materialList[material][4])
        self.builder.get_object('cutFeedRateAdj').set_value(Global.materialList[material][5])
        self.builder.get_object('cutAmpsAdj').set_value(Global.materialList[material][6])
        self.builder.get_object('cutVoltsAdj').set_value(Global.materialList[material][7])

    def wait_for_completion(self):
        while self.lcnc.comd.wait_complete() == -1:
            pass

    def on_xToHome_clicked(self,event):
        self.goto_home('X')

    def on_yToHome_clicked(self,event):
        self.goto_home('Y')

    def on_zToHome_clicked(self,event):
        self.goto_home('Z')

    def goto_home(self,axis):
        idle = sp.Popen(['halcmd getp halui.program.is-idle'], stdout=sp.PIPE, shell=True).communicate()[0].strip()
        if idle == 'TRUE':
            home = self.lcnc.linuxcncIniFile.find('JOINT_' + str(self.lcnc.linuxcncIniFile.find('TRAJ', 'COORDINATES').upper().index(axis)), 'HOME')
            mode = sp.Popen(['halcmd getp halui.mode.is-mdi'], stdout=sp.PIPE, shell=True).communicate()[0].strip()
            if mode == 'FALSE':
                self.lcnc.comd.mode(linuxcnc.MODE_MDI)
                self.wait_for_completion()
            self.lcnc.comd.mdi('G53 G0 ' + axis + home)
            self.wait_for_completion()
            if mode == 'FALSE':
                self.lcnc.comd.mode(linuxcnc.MODE_MANUAL)
                self.wait_for_completion()

    def configure_widgets(self):
        # set_digits = number of digits after decimal
        # configure  = (value, lower limit, upper limit, step size, 0, 0)
        self.builder.get_object('arcFailDelay').set_digits(1)
        self.builder.get_object('arcFailDelayAdj').configure(1,0.1,60,0.1,0,0)
        self.builder.get_object('arcOkLow').set_digits(1)
        self.builder.get_object('arcOkLowAdj').configure(0,0,200,0.5,0,0)
        self.builder.get_object('arcOkHigh').set_digits(1)
        self.builder.get_object('arcOkHighAdj').configure(0,0,200,0.5,0,0)
        self.builder.get_object('arcMaxStarts').set_digits(0)
        self.builder.get_object('arcMaxStartsAdj').configure(3,1,9,1,0,0)
        self.builder.get_object('arcRestartDelay').set_digits(0)
        self.builder.get_object('arcRestartDelayAdj').configure(1,1,60,1,0,0)
        self.builder.get_object('arcVoltageOffset').set_digits(1)
        self.builder.get_object('arcVoltageOffsetAdj').configure(0,-100,100,0.1,0,0)
        self.builder.get_object('arcVoltageScale').set_digits(2)
        self.builder.get_object('arcVoltageScaleAdj').configure(1,0.01,99,0.01,0,0)
        self.builder.get_object('cornerlockEnable').set_active(1)
        self.builder.get_object('cornerlockThreshold').set_digits(0)
        self.builder.get_object('cornerlockThresholdAdj').configure(90,1,99,1,0,0)
        self.builder.get_object('cutAmps').set_digits(0)
        self.builder.get_object('cutAmpsAdj').configure(45,0,999,1,0,0)
        self.builder.get_object('cutVolts').set_digits(1)
        self.builder.get_object('cutVoltsAdj').configure(122,0,300,0.1,0,0)
        self.builder.get_object('kerfCrossEnable').set_active(1)
        self.builder.get_object('kerfCrossThreshold').set_digits(1)
        self.builder.get_object('kerfCrossThresholdAdj').configure(3,0,9,0.1,0,0)
        self.builder.get_object('maxOffsetVelocityIn').set_label(str(int(Global.thcFeedRate)))
        self.builder.get_object('pidPGain').set_digits(0)
        self.builder.get_object('pidPGainAdj').configure(25,0,1000,1,0,0)
        self.builder.get_object('pidIGain').set_digits(0)
        self.builder.get_object('pidIGainAdj').configure(0,0,1000,1,0,0)
        self.builder.get_object('pidDGain').set_digits(0)
        self.builder.get_object('pidDGainAdj').configure(0,0,1000,1,0,0)
        self.builder.get_object('pierceDelay').set_digits(1)
        self.builder.get_object('pierceDelayAdj').configure(0.5,0,9,0.1,0,0)
        self.builder.get_object('puddleJumpDelay').set_digits(2)
        self.builder.get_object('puddleJumpDelayAdj').configure(0,0,9,0.01,0,0)
        self.builder.get_object('thcEnable').set_active(1)
        self.builder.get_object('thcThreshold').set_digits(2)
        self.builder.get_object('thcThresholdAdj').configure(1,0.05,9,0.01,0,0)
        self.builder.get_object('torchOffDelay').set_digits(1)
        self.builder.get_object('torchOffDelayAdj').configure(0.1,0,9,0.1,0,0)
        self.builder.get_object('useAutoVolts').set_active(1)
        if self.lcnc.linuxcncIniFile.find('TRAJ', 'LINEAR_UNITS').lower() == 'mm':
            self.builder.get_object('cutFeedRate').set_digits(0)
            self.builder.get_object('cutFeedRateAdj').configure(4000,0,9999,1,0,0)
            self.builder.get_object('cutHeight').set_digits(2)
            self.builder.get_object('cutHeightAdj').configure(0.76,0.1,9,0.01,0,0)
            self.builder.get_object('floatSwitchTravel').set_digits(2)
            self.builder.get_object('floatSwitchTravelAdj').configure(1.5,0,9,0.01,0,0)
            self.builder.get_object('pierceHeight').set_digits(2)
            self.builder.get_object('pierceHeightAdj').configure(3.8,0.1,9,0.01,0,0)
            self.builder.get_object('probeFeedRate').set_digits(0)
            self.builder.get_object('probeFeedRateAdj').configure(1000,1,5000,1,0,0)
            self.builder.get_object('safeHeight').set_digits(0)
            self.builder.get_object('safeHeightAdj').configure(20,1,99,1,0,0)
            self.builder.get_object('setupFeedRate').set_digits(0)
            self.builder.get_object('setupFeedRateAdj').configure(int(Global.thcFeedRate * 0.8),1,Global.thcFeedRate,1,0,0)
        elif self.lcnc.linuxcncIniFile.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch':
            self.builder.get_object('cutFeedRate').set_digits(1)
            self.builder.get_object('cutFeedRateAdj').configure(160,0,400,0.1,0,0)
            self.builder.get_object('cutHeight').set_digits(3)
            self.builder.get_object('cutHeightAdj').configure(0.03,0.01,.5,0.001,0,0)
            self.builder.get_object('floatSwitchTravel').set_digits(3)
            self.builder.get_object('floatSwitchTravelAdj').configure(0.06,0,1,0.001,0,0)
            self.builder.get_object('pierceHeight').set_digits(3)
            self.builder.get_object('pierceHeightAdj').configure(0.15,0.01,1,0.001,0,0)
            self.builder.get_object('probeFeedRate').set_digits(1)
            self.builder.get_object('probeFeedRateAdj').configure(40,.1,400,.1,0,0)
            self.builder.get_object('safeHeight').set_digits(2)
            self.builder.get_object('safeHeightAdj').configure(0.75,0.04,4,0.01,0,0)
            self.builder.get_object('setupFeedRate').set_digits(1)
            self.builder.get_object('setupFeedRateAdj').configure(int(Global.thcFeedRate * 0.8),.1,Global.thcFeedRate,.1,0,0)
        else:
            print '\nIncorrect [TRAJ]LINEAR_UNITS in ini file\n'

    def mode_check(self):
        mode = int((sp.Popen(['halcmd getp plasmac.mode'], stdout=sp.PIPE, shell=True)).communicate()[0].strip())
        units = float(sp.Popen(['halcmd getp halui.machine.units-per-mm'], stdout=sp.PIPE, shell=True).communicate()[0].strip())
        maxPidP = Global.thcFeedRate / units * 0.1
        if mode != Global.oldMode:
            if mode == 0:
                self.builder.get_object('arcOkHigh').show()
                self.builder.get_object('arcOkHighLabel').set_text('OK High Voltage')
                self.builder.get_object('arcOkLow').show()
                self.builder.get_object('arcOkLowLabel').set_text('OK Low Voltage')
                self.builder.get_object('arcVoltage').show()
                self.builder.get_object('arcVoltageLabel').set_text('Arc Voltage')
                self.builder.get_object('arcVoltageScale').show()
                self.builder.get_object('arcVoltageScaleLabel').set_text('Voltage Scale')
                self.builder.get_object('arcVoltageOffset').show()
                self.builder.get_object('arcVoltageOffsetLabel').set_text('Voltage Offset')
                self.builder.get_object('autoBox').show()
                self.builder.get_object('cutBlankLabel').show()
                self.builder.get_object('heightFrame').show()
                self.builder.get_object('kerfBox').show()
                self.builder.get_object('kerfLabel').show()
                self.builder.get_object('kerfFrame').set_shadow_type(gtk.SHADOW_OUT)
                self.builder.get_object('pidPGainAdj').configure(self.builder.get_object('pidPGainAdj').get_value(),1,maxPidP,1,0,0)
                self.builder.get_object('pidPLabel').set_text('Speed (PID P)')
                self.builder.get_object('pidIGain').show()
                self.builder.get_object('pidILabel').set_text('PID I GAIN')
                self.builder.get_object('pidDGain').show()
                self.builder.get_object('pidDLabel').set_text('PID D GAIN')
                self.builder.get_object('thresholdBox').show()
                self.builder.get_object('voltsBox').show()
            elif mode == 1:
                self.builder.get_object('arcOkHigh').hide()
                self.builder.get_object('arcOkHighLabel').set_text('')
                self.builder.get_object('arcOkLow').hide()
                self.builder.get_object('arcOkLowLabel').set_text('')
                self.builder.get_object('arcVoltage').show()
                self.builder.get_object('arcVoltageLabel').set_text('Arc Voltage')
                self.builder.get_object('arcVoltageScale').show()
                self.builder.get_object('arcVoltageScaleLabel').set_text('Voltage Scale')
                self.builder.get_object('arcVoltageOffset').show()
                self.builder.get_object('arcVoltageOffsetLabel').set_text('Voltage Offset')
                self.builder.get_object('autoBox').show()
                self.builder.get_object('cutBlankLabel').show()
                self.builder.get_object('heightFrame').show()
                self.builder.get_object('kerfBox').show()
                self.builder.get_object('kerfLabel').show()
                self.builder.get_object('kerfFrame').set_shadow_type(gtk.SHADOW_OUT)
                self.builder.get_object('pidPGainAdj').configure(self.builder.get_object('pidPGainAdj').get_value(),1,maxPidP,1,0,0)
                self.builder.get_object('pidPLabel').set_text('Speed (PID P)')
                self.builder.get_object('pidIGain').show()
                self.builder.get_object('pidILabel').set_text('PID I GAIN')
                self.builder.get_object('pidDGain').show()
                self.builder.get_object('pidDLabel').set_text('PID D GAIN')
                self.builder.get_object('thresholdBox').show()
                self.builder.get_object('voltsBox').show()
            elif mode == 2:
                self.builder.get_object('arcOkHigh').hide()
                self.builder.get_object('arcOkHighLabel').set_text('')
                self.builder.get_object('arcOkLow').hide()
                self.builder.get_object('arcOkLowLabel').set_text('')
                self.builder.get_object('arcVoltage').hide()
                self.builder.get_object('arcVoltageLabel').set_text('')
                self.builder.get_object('arcVoltageScale').hide()
                self.builder.get_object('arcVoltageScaleLabel').set_text('')
                self.builder.get_object('arcVoltageOffset').hide()
                self.builder.get_object('arcVoltageOffsetLabel').set_text('')
                self.builder.get_object('autoBox').hide()
                self.builder.get_object('cutBlankLabel').hide()
                self.builder.get_object('heightFrame').hide()
                self.builder.get_object('kerfBox').hide()
                self.builder.get_object('kerfLabel').hide()
                self.builder.get_object('kerfFrame').set_shadow_type(gtk.SHADOW_NONE)
                self.builder.get_object('pidPGainAdj').configure(self.builder.get_object('pidPGainAdj').get_value(),1,100,1,0,0)
                self.builder.get_object('pidPLabel').set_text('Speed (%)')
                self.builder.get_object('pidIGain').hide()
                self.builder.get_object('pidILabel').set_text('')
                self.builder.get_object('pidDGain').hide()
                self.builder.get_object('pidDLabel').set_text('')
                self.builder.get_object('thresholdBox').hide()
                self.builder.get_object('voltsBox').hide()
            else:
                pass
            Global.oldMode = mode
        return True

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.lcnc = linuxcncInterface()
        gtk.settings_get_default().set_property('gtk-theme-name', self.lcnc.linuxcncIniFile.find('PLASMAC', 'THEME'))
        configEnable = self.lcnc.linuxcncIniFile.find('PLASMAC', 'CONFIG_ENABLE') or '1'
        sp.Popen(['halcmd setp gladevcp.configEnable ' + configEnable], shell=True)
        Global.thcFeedRate = (float(self.lcnc.linuxcncIniFile.find('AXIS_Z', 'MAX_VELOCITY')) * \
                              float(self.lcnc.linuxcncIniFile.find('AXIS_Z', 'OFFSET_AV_RATIO'))) * 60
        sp.Popen('halcmd setp plasmac.thc-feed-rate %f' % Global.thcFeedRate, shell=True)
        self.ini_filename = self.lcnc.linuxcncIniFile.find('PLASMAC', 'CONFIG_FILE') or \
                            self.lcnc.linuxcncIniFile.find('EMC', 'MACHINE').lower() + '.cfg'
        self.configure_widgets()
        self.defaults = {IniFile.vars: dict(), \
                         IniFile.widgets: widget_defaults(select_widgets(self.builder.get_objects(), hal_only=False,output_only = True))}
        self.ini = IniFile(self.ini_filename,self.defaults,self.builder)
        self.ini.restore_state(self)
        p_height = self.builder.get_object('pierceHeightAdj').get_value()
        p_delay = self.builder.get_object('pierceDelayAdj').get_value()
        pj_delay = self.builder.get_object('puddleJumpDelayAdj').get_value()
        c_height = self.builder.get_object('cutHeightAdj').get_value()
        c_speed = self.builder.get_object('cutFeedRateAdj').get_value()
        c_amps = self.builder.get_object('cutAmpsAdj').get_value()
        c_volts = self.builder.get_object('cutVoltsAdj').get_value()
        Global.materialList.append(['Default', p_height, p_delay, pj_delay, c_height, c_speed, c_amps, c_volts])
        materialFile = self.lcnc.linuxcncIniFile.find('PLASMAC', 'MATERIAL_FILE') or self.lcnc.linuxcncIniFile.find('EMC', 'MACHINE').lower() + '.mat'
        self.builder.get_object('material').set_active(0)
        if os.path.exists(materialFile):
            try:
                with open(materialFile, 'r') as f:
                    for line in f:
                        if not line.startswith('#'):
                            name, p_height, p_delay, pj_delay, c_height, c_speed, c_amps, c_volts = line.split(',')
                            name = name.strip()
                            p_height = float(p_height)
                            p_delay = float(p_delay)
                            pj_delay = float(pj_delay)
                            c_height = float(c_height)
                            c_speed = float(c_speed)
                            c_amps = float(c_amps)
                            c_volts = float(c_volts)
                            iter = self.builder.get_object('materials').append()
                            self.builder.get_object('materials').set(iter, 0, name, 1, p_height, 2, p_delay, 3, pj_delay, 4, c_height, 5, c_speed, 6, c_amps, 7, c_volts)
                            Global.materialList.append([name, p_height, p_delay, pj_delay, c_height, c_speed, c_amps, c_volts])
            except:
                print '\n*** MATERIAL FILE,', materialFile, ' IS INVALID ***\n'
        else:
            with open(materialFile, 'w') as f:
                f.write('#   all fields must exist...\n#   Format =:\n#   name,   pierce height,   pierce-delay,   puddle-jump-delay, cut-height,   cut-speed,   cut-amps,   cut-volts\n#\n')
            print '\n*** EMPTY MATERIAL FILE,', materialFile, ' CREATED ***\n'
        gobject.timeout_add(100, self.mode_check)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
