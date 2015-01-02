#!/usr/bin/env python

import linuxcnc
import hal
import time
import sys
import os
import math


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


def wait_for_joint_to_stop(joint_number):
    pos_pin = 'joint-%d-position' % joint_number
    start_time = time.time()
    timeout = 2.0
    prev_pos = h[pos_pin]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        new_pos = h[pos_pin]
        if new_pos == prev_pos:
            return
        prev_pos = new_pos
    print "Error: joint didn't stop jogging!"
    print "joint %d is at %.6f %.6f seconds after reaching target (prev_pos=%.6f)" % (joint_number, h[pos_pin], timeout, prev_pos)
    sys.exit(1)


def close_enough(a, b, epsilon=0.000001):
    return math.fabs(a - b) < epsilon


def jog_joint(joint_number, counts=1, scale=0.001):
    timeout = 5.0

    start_pos = 3*[0]
    for j in range(0,3):
        start_pos[j] = h['joint-%d-position' % j]

    target = h['joint-%d-position' % joint_number] + (counts * scale)

    h['joint-%d-jog-scale' % joint_number] = scale
    h['joint-%d-jog-enable' % joint_number] = 1
    h['joint-%d-jog-counts' % joint_number] += counts

    start_time = time.time()
    while not close_enough(h['joint-%d-position' % joint_number], target) and (time.time() - start_time < timeout):
        #print "joint %d is at %.9f" % (joint_number, h['joint-%d-position' % joint_number])
        time.sleep(0.010)

    h['joint-%d-jog-enable' % joint_number] = 0

    print "joint jogged from %.6f to %.6f (%d counts at scale %.6f)" % (start_pos[joint_number], h['joint-%d-position' % joint_number], counts, scale)

    success = True
    for j in range(0,3):
        pin_name = 'joint-%d-position' % j
        if j == joint_number:
            if not close_enough(h[pin_name], target):
                print "joint %d didn't get to target (start=%.6f, target=%.6f, got to %.6f)" % (joint_number, start_pos[joint_number], target, h['joint-%d-position' % joint_number])
                success = False
        else:
            if h[pin_name] != start_pos[j]:
                print "joint %d moved from %.6f to %.6f but shouldnt have!" % (j, start_pos[j], h[pin_name])
                success = False

    wait_for_joint_to_stop(joint_number)

    if not success:
        sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("test-ui")

h.newpin("joint-0-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-0-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-1-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-1-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-2-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-2-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.set_mode(linuxcnc.MODE_MANUAL)


#
# run the test
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong.
#

jog_joint(0, counts=1, scale=0.001)
jog_joint(1, counts=10, scale=-0.025)
jog_joint(2, counts=-100, scale=0.100)

sys.exit(0)

