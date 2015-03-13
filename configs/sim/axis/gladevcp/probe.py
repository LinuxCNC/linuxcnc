#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of Machinekit
#    probe.py Copyright 2010 Michael Haberler
#
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA''''''
'''
    gladevcp probe demo example
    Michael Haberler 11/2010

'''

import os,sys
from gladevcp.persistence import IniFile,widget_defaults,set_debug,select_widgets
import hal
import hal_glib
import gtk
import glib

import linuxcnc

debug = 0


class EmcInterface(object):

    def __init__(self):
        try:
            self.s = linuxcnc.stat();
            self.c = linuxcnc.command()
        except Exception, msg:
            print "cant initialize EmcInterface: %s - Machinekit not running?" %(msg)

    def running(self,do_poll=True):
        if do_poll: self.s.poll()
        return self.s.task_mode == linuxcnc.MODE_AUTO and self.s.interp_state != linuxcnc.INTERP_IDLE

    def manual_ok(self,do_poll=True):
        if do_poll: self.s.poll()
        if self.s.task_state != linuxcnc.STATE_ON: return False
        return self.s.interp_state == linuxcnc.INTERP_IDLE


    def ensure_mode(self,m, *p):
        '''
        If Machinekit is not already in one of the modes given, switch it to the first mode
        example:
        ensure_mode(linuxcnc.MODE_MDI)
        ensure_mode(linuxcnc.MODE_AUTO, linuxcnc.MODE_MDI)
        '''
        self.s.poll()
        if self.s.task_mode == m or self.s.task_mode in p: return True
        if self.running(do_poll=False): return False
        self.c.mode(m)
        self.c.wait_complete()
        return True

    def active_codes(self):
        self.s.poll()
        return self.s.gcodes

    def get_current_system(self):
        for i in self.active_codes():
                if i >= 540 and i <= 590:
                        return i/10 - 53
                elif i >= 590 and i <= 593:
                        return i - 584
        return 1


    def mdi_command(self,command, wait=True):
        #ensure_mode(emself.c.MODE_MDI)
        self.c.mdi(command)
        if wait: self.c.wait_complete()

    def emc_status(self):
        '''
        return tuple (task mode, task state, exec state, interp state) as strings
        '''
        self.s.poll()
        task_mode = ['invalid', 'MANUAL', 'AUTO', 'MDI'][self.s.task_mode]
        task_state = ['invalid', 'ESTOP', 'ESTOP_RESET', 'OFF', 'ON'][self.s.task_state]
        exec_state = ['invalid', 'ERROR', 'DONE',
              'WAITING_FOR_MOTION',
              'WAITING_FOR_MOTION_QUEUE',
              'WAITING_FOR_IO',
              'WAITING_FOR_PAUSE',
              'WAITING_FOR_MOTION_AND_IO',
              'WAITING_FOR_DELAY',
              'WAITING_FOR_SYSTEM_CMD' ][self.s.exec_state]
        interp_state = ['invalid', 'IDLE', 'READING', 'PAUSED', 'WAITING'][self.s.interp_state]
        return (task_mode, task_state, exec_state, interp_state)



class HandlerClass:

    def on_manual_mode(self,widget,data=None):
        if self.e.ensure_mode(linuxcnc.MODE_MANUAL):
            print "switched to manual mode"
        else:
            print "cant switch to manual in this state"

    def on_mdi_mode(self,widget,data=None):
        if self.e.ensure_mode(linuxcnc.MODE_MDI):
            print "switched to MDI mode"
        else:
            print "cant switch to MDI in this state"

    def _query_emc_status(self,data=None):
        (task_mode, task_state, exec_state, interp_state) = self.e.emc_status()
        self.builder.get_object('task_mode').set_label("Task mode: " + task_mode)
        self.builder.get_object('task_state').set_label("Task state: " + task_state)
        self.builder.get_object('exec_state').set_label("Exec state: " + exec_state)
        self.builder.get_object('interp_state').set_label("Interp state: " + interp_state)
        return True

    def on_probe(self,widget,data=None):
        label = widget.get_label()
        axis = ord(label[0].lower()) - ord('x')
        direction = 1.0
        if label[1] == '-':
            direction = -1.0
        self.e.s.poll()
        self.start_feed = self.e.s.settings[1]
        # determine system we are touching off - 1...g54 etc
        self.current_system = self.e.get_current_system()
        # remember current abs or rel mode -  g91
        self.start_relative = (910 in self.e.active_codes())

        self.previous_mode = self.e.s.task_mode
        if self.e.s.task_state != linuxcnc.STATE_ON:
            print "machine not turned on"
            return
        if not self.e.s.homed[axis]:
            print "%s axis not homed" %(chr(axis + ord('X')))
            return
        if self.e.running(do_poll=False):
            print "cant do that now - intepreter running"
            return

        self.e.ensure_mode(linuxcnc.MODE_MDI)
        self.e.mdi_command("#<_Probe_System> = %d " % (self.current_system ),wait=False)
        self.e.mdi_command("#<_Probe_Axis> = %d " % (axis),wait=False)
        self.e.mdi_command("#<_Probe_Speed> = %s " % (self.builder.get_object('probe_feed').get_value()),wait=False)
        self.e.mdi_command("#<_Probe_Diameter> = %s " % (self.builder.get_object('probe_diameter').get_value() ),wait=False)
        self.e.mdi_command("#<_Probe_Distance> = %s " % (self.builder.get_object('probe_travel').get_value() * direction),wait=False)
        self.e.mdi_command("#<_Probe_Retract> = %s " % (self.builder.get_object('retract').get_value() * direction * -1.0),wait=False)
        self.e.mdi_command("O<probe> call",wait=False)

        self.e.mdi_command('F%f' % (self.start_feed),wait=False)
        self.e.mdi_command('G91' if self.start_relative else 'G90',wait=False)
#        self.e.ensure_mode(self.previous_mode)


    def on_destroy(self,obj,data=None):
        self.ini.save_state(self)

    def on_restore_defaults(self,button,data=None):
        '''
        example callback for 'Reset to defaults' button
        currently unused
        '''
        self.ini.create_default_ini()
        self.ini.restore_state(self)


    def __init__(self, halcomp,builder,useropts,compname):
        self.halcomp = halcomp
        self.builder = builder

        self.ini_filename = __name__ + '.save'
        self.defaults = {  IniFile.vars: dict(),
                           IniFile.widgets : widget_defaults(select_widgets(self.builder.get_objects(), hal_only=False,output_only = True))
                        }
        self.ini = IniFile(self.ini_filename,self.defaults,self.builder)
        self.ini.restore_state(self)

        self.e = EmcInterface()

        glib.timeout_add_seconds(1, self._query_emc_status)

def get_handlers(halcomp,builder,useropts,compname):

    global debug
    for cmd in useropts:
        exec cmd in globals()

    set_debug(debug)

    return [HandlerClass(halcomp,builder,useropts,compname)]
