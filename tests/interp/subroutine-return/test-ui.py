#!/usr/bin/env python

import linuxcnc
import hal

import time
import sys
import os


# this is how long we wait for linuxcnc to do our bidding
timeout = 1.0


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
                if self.g_raise_except:
                    raise LinuxcncError(text)
                else:        
                    print ("error " + text)
            else:
                print ("info " + text)
        return False

    def get_current_tool(self):
        self.s.poll()
        return self.s.tool_in_spindle

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









#
# set up pins
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("python-ui")
h.ready()


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.g_raise_except = False
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.c.home(0)
e.c.home(1)
e.c.home(2)

start_time = time.time()
while (time.time() - start_time) < 2.0:
    e.s.poll()
    if e.s.homed == ( 1, 1, 1, 0, 0, 0, 0, 0, 0 ):
        break
    time.sleep(0.1)

if e.s.homed != ( 1, 1, 1, 0, 0, 0, 0, 0, 0 ):
    print "failed to home in 2 seconds"
    sys.exit(1)

e.set_mode(linuxcnc.MODE_AUTO)

e.c.program_open("test.ngc")
e.c.auto(linuxcnc.AUTO_RUN, 1)

start_time = time.time()
while (time.time() - start_time) < 2.0:
    e.s.poll()
    if e.s.interp_state != linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.001)

if e.s.interp_state == linuxcnc.INTERP_IDLE:
    print "failed to start interpreter, interp_state is", e.s.interp_state
    sys.exit(1)

# tee hee!
os.rename('test.ngc', 'moved-test.ngc')
#os.rename('subs/sub.ngc', 'subs/moved-sub.ngc')

start_time = time.time()
while (time.time() - start_time) < 2.0:
    e.s.poll()
    if e.s.interp_state == linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.001)

if e.s.interp_state != linuxcnc.INTERP_IDLE:
    print "interpreter did not finish"
    sys.exit(1)

# ok fine, have it back
os.rename('moved-test.ngc', 'test.ngc')
#os.rename('subs/moved-sub.ngc', 'subs/sub.ngc')


print "done! it all worked"

# if we get here it all worked!
sys.exit(0)

