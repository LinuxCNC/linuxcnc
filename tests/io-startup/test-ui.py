#!/usr/bin/env python

import linuxcnc
import hal
import time
import sys
import os


# this is how long we wait for linuxcnc to do our bidding
timeout = 5.0


# unbuffer stdout
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)


class LinuxcncError(Exception):
    pass
#    def __init__(self, value):
#        self.value = value
#    def __str__(self):
#        return repr(self.value)

class LinuxcncControl:
    '''
    issue G-Code commands
    make sure important modes are saved and restored
    mode is saved only once, and can be restored only once
    
    usage example: 
        e = emc_control()
        e.prepare_for_mdi()
            any internal sub using e.g("G0.....")
        e.finish_mdi()
    
    '''

    def __init__(self):
        self.c = linuxcnc.command()
        self.e = linuxcnc.error_channel()
        self.s = linuxcnc.stat()
        
    def running(self, do_poll=True):
        '''
        check wether interpreter is running.
        If so, cant switch to MDI mode.
        '''
        if do_poll: 
            self.s.poll()
        return (self.s.task_mode == linuxcnc.MODE_AUTO and 
                self.s.interp_state != linuxcnc.INTERP_IDLE)
                
    def set_mode(self,m):
        '''
        set EMC mode if possible, else throw LinuxcncError
        return current mode
        '''
        self.s.poll()
        if self.s.task_mode == m : 
            return m
        if self.running(do_poll=False): 
            raise LinuxcncError("interpreter running - cant change mode")
        self.c.mode(m)   
        self.c.wait_complete()
        return m  

    def set_state(self,m):
        '''
        set EMC mode if possible, else throw LinuxcncError
        return current mode
        '''
        self.s.poll()
        if self.s.task_mode == m : 
            return m
        self.c.state(m)   
        self.c.wait_complete()
        return m

    def do_home(self,axismask):
        self.s.poll()
        self.c.home(axismask)   
        self.c.wait_complete()


    def ok_for_mdi(self):
        ''' 
        check wether ok to run MDI commands.
        '''
        self.s.poll()
        return not self.s.estop and self.s.enabled and self.s.homed 
        
    def prepare_for_mdi(self):
        ''' 
        check wether ok to run MDI commands.
        throw  LinuxcncError if told so.
        return current mode
        '''

        self.s.poll()
        if self.s.estop:
            raise LinuxcncError("machine in ESTOP")
        
        if not self.s.enabled:
            raise LinuxcncError("machine not enabled")
        
        if not self.s.homed:
            raise LinuxcncError("machine not homed")
        
        if self.running():
            raise LinuxcncError("interpreter not idle")
            
        return self.set_mode(linuxcnc.MODE_MDI)

    g_raise_except = True
    
    def g(self,code,wait=False):
        '''
        issue G-Code as MDI command.
        wait for completion if reqested
        '''
        
        self.c.mdi(code)
        if wait:
            try:
                while self.c.wait_complete() == -1:
                    pass
                return True
            except KeyboardInterrupt:
                print "interrupted by keyboard in c.wait_complete()"
                return False

        self.error = self.e.poll()
        if self.error:
            kind, text = self.error
            if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                if LinuxcncControl.g_raise_except:
                    raise LinuxcncError(text)
                else:        
                    print ("error " + text)
            else:
                print ("info " + text)
        return False

    def get_current_tool(self):
        self.e.poll()
        return self.e.tool_in_spindle

    def active_codes(self):
        self.e.poll()
        return self.s.gcodes
    
    def get_current_system(self):
        g = self.active_codes()
        for i in g:
                if i >= 540 and i <= 590:
                        return i/10 - 53
                elif i >= 590 and i <= 593:
                        return i - 584
        return 1

    def tool_in_spindle(self):
        self.s.poll()
        return self.s.tool_in_spindle


def wait_for_pin_value(pin_name, value, timeout=5.0):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        if h[pin_name] == value:
            print "pin '%s' reached target value '%s' after %f seconds" % (pin_name, value, time.time() - start_time)
            return
    print "Error: pin '%s' didn't reach value '%s' within timeout of %f seconds (it's %s instead)" % (pin_name, value, timeout, h[pin_name])
    sys.exit(1)


def wait_for_stat_buffer_toolInSpindle(value, timeout=5.0):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        if e.tool_in_spindle() == expected_startup_tool_number:
            print "the Stat buffer's toolInSpindle reached the value of %d after %f seconds" % (expected_startup_tool_number, time.time() - start_time)
            return
    print "the Stat buffer's toolInSpindle value is %d, expected %d" % (e.tool_in_spindle(), expected_startup_tool_number)
    sys.exit(1)


f = open('expected-startup-tool-number', 'r')
contents = f.read()
f.close()
expected_startup_tool_number = int(contents)
print "expecting tool number %d" % expected_startup_tool_number

#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("test-ui")

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ../../postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()

wait_for_pin_value('tool-number', expected_startup_tool_number)

wait_for_stat_buffer_toolInSpindle(expected_startup_tool_number)

sys.exit(0)

