#!/usr/bin/env python

import linuxcnc
import hal
import time
import sys
import os
import math


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


def introspect():
    #print "joint.0.select =", h['joint-0-select']
    #print "joint.0.selected =", h['joint-0-selected']
    #print "joint.0.position =", h['joint-0-position']
    os.system("halcmd show pin halui")
    os.system("halcmd show pin python-ui")
    os.system("halcmd show sig")


def wait_for_joint_to_stop_at(joint, target):
    timeout = 5.0
    tolerance = 0.0001

    start = time.time()

    curr_pos = 0;
    while ((time.time() - start) < timeout):
        prev_pos = curr_pos
        curr_pos = h['joint-%d-position' % joint]
        vel = curr_pos - prev_pos
        error = math.fabs(curr_pos - target)
        if (error < tolerance) and (vel == 0):
            print "joint %d stopped at %.3f" % (joint, target)
            return
        time.sleep(0.1)
    print "timeout waiting for joint %d to stop at %.3f (pos=%.3f, vel=%.3f)" % (joint, target, curr_pos, vel)
    sys.exit(1)


def wait_for_task_mode(target):
    timeout = 5.0
    start = time.time()

    while ((time.time() - start) < timeout):
        e.s.poll()
        if e.s.task_mode == target:
            return
        time.sleep(0.1)

    print "timeout waiting for task mode to get to  %d (it's %d)" % (target, e.s.task_mode)
    sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("python-ui")

h.newpin("mdi-0", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("mdi-1", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("mdi-2", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("mdi-3", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)


#
# run the test
#
# These functions will exit with a return value of 1 if something goes
# wrong.
#

e.set_mode(linuxcnc.MODE_MANUAL)
print "running MDI command 0"
h['mdi-0'] = 1
wait_for_joint_to_stop_at(0, 0);
wait_for_joint_to_stop_at(1, 0);
wait_for_joint_to_stop_at(2, 0);
h['mdi-0'] = 0
wait_for_task_mode(linuxcnc.MODE_MANUAL)

e.set_mode(linuxcnc.MODE_AUTO)
print "running MDI command 1"
h['mdi-1'] = 1
wait_for_joint_to_stop_at(0, 1);
wait_for_joint_to_stop_at(1, 0);
wait_for_joint_to_stop_at(2, 0);
h['mdi-1'] = 0
wait_for_task_mode(linuxcnc.MODE_AUTO)

e.set_mode(linuxcnc.MODE_MDI)
print "running MDI command 2"
h['mdi-2'] = 1
wait_for_joint_to_stop_at(0, 1);
wait_for_joint_to_stop_at(1, 2);
wait_for_joint_to_stop_at(2, 0);
h['mdi-2'] = 0
e.s.poll()
wait_for_task_mode(linuxcnc.MODE_MDI)

print "running MDI command 3"
h['mdi-3'] = 1
wait_for_joint_to_stop_at(0, 1);
wait_for_joint_to_stop_at(1, 2);
wait_for_joint_to_stop_at(2, 3);
h['mdi-3'] = 0
wait_for_task_mode(linuxcnc.MODE_MDI)

print "running MDI command 0"
h['mdi-0'] = 1
wait_for_joint_to_stop_at(0, 0);
wait_for_joint_to_stop_at(1, 0);
wait_for_joint_to_stop_at(2, 0);
h['mdi-0'] = 0
wait_for_task_mode(linuxcnc.MODE_MDI)


sys.exit(0)

