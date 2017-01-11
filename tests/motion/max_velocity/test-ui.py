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


def close_enough(a, b, epsilon=0.001):
    return math.fabs(a - b) < epsilon


def reset_minmax(h):
    h['minmax-reset'] = 1
    time.sleep(0.1)  # Time for reset to be recognized
    if h['XYZmax'] != 0 or h['ABCmax'] != 0:
        print "Error:  max velocity failed to reset:  " \
            "XYZmax=%.3f, ABCmax=%.3f" % \
            (h['XYZmax'], h['ABCmax'])
        # os.system('halcmd show pin')
        sys.exit(1)
    h['minmax-reset'] = 0

jointmap = { 0:'X', 1:'Y', 2:'Z', 3:'A', 4:'B', 5:'C' }

def move_joints(e, h, maxvel_set, maxvel_expected=None,
                X=0, Y=0, Z=0, A=0, B=0, C=0):
    if maxvel_expected is None:  maxvel_expected = maxvel_set

    # Setup:  reset minmax comp; set max velocity; MDI mode
    reset_minmax(h)
    e.c.maxvel(maxvel_set, maxvel_set)
    e.prepare_for_mdi()

    # Build and run command
    cmd = 'G0 X%.3f Y%.3f Z%.3f A%.3f B%.3f C%.3f' % \
          (X, Y, Z, A, B, C)
    e.g(cmd, wait=True)

    # Check that set max velocity was reached on XYZ or ABC axes,
    # depending on whether the motion was pure rotary or not
    pure_rotary = (X==0 and Y==0 and Z==0)
    maxvel_pin = 'ABCmax' if pure_rotary else 'XYZmax'
    maxvel = h[maxvel_pin]
    print "cmd = %s" % cmd
    print "  max %s vel: set = %6.3f, measured = %6.3f, expecting = %6.3f" % \
        ('ang' if pure_rotary else 'lin',
         maxvel_set, maxvel, maxvel_expected)
    # print "  ending coords:  X=%6.3f Y=%6.3f Z=%6.3f A=%6.3f B=%6.3f C=%6.3f " % \
    #     tuple([ h['joint-%d-position' % i] for i in range(6) ])
    print "  cartesian max vel           X=%6.3f Y=%6.3f Z=%6.3f => %6.3f" % \
        tuple([h['joint-%d-velocity' % i] for i in range(0,3)] + [h['XYZmax']])
    print "  angular max vel             A=%6.3f B=%6.3f C=%6.3f => %6.3f" % \
        tuple([h['joint-%d-velocity' % i] for i in range(3,6)] + [h['ABCmax']])
    if not close_enough(maxvel, maxvel_expected):
        print "Error:  max velocity %.3f != set %.3f" % \
            (maxvel, maxvel_expected)
        sys.exit(1)

    # Go back to zero
    e.c.maxvel(10800, 10800)
    e.g('G0 X0 Y0 Z0 A0 B0 C0', wait=True)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("test-ui")

h.newpin("joint-0-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-0-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-0-velocity", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-1-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-1-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-1-velocity", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-2-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-2-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-2-velocity", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-3-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-3-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-3-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-3-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-3-velocity", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-4-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-4-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-4-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-4-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-4-velocity", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-5-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-5-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-5-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-5-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-5-velocity", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("XYZmax", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("ABCmax", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("minmax-reset", hal.HAL_BIT, hal.HAL_OUT)

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
# These move_joints() functions will exit with a return value of 1 if
# something goes wrong.
#

# INI file max velocities:
#   X 4.0; Y 4.1; Z 4.2; A 90.3; B 90.4; C 90.5
#   MAX_VELOCITY 4.5; MAX_ANGULAR_VELOCITY 95.0

# X-axis only, pure linear; limited by joint max 4.0 ipm
move_joints(e, h, 9, 4, X=0.02)
move_joints(e, h, 4, 4, X=0.02)
move_joints(e, h, 3, 3, X=0.02)
move_joints(e, h, 1, 1, X=0.02)

# XYZ axes, pure linear; same distance, limited to hypot of lowest
# joint max sqrt(4^2 * 3)
move_joints(e, h, 9.000, 6.928, X=0.02, Y=0.02, Z=0.02)
move_joints(e, h, 6.928, 6.928, X=0.02, Y=0.02, Z=0.02)
move_joints(e, h, 4.000, 4.000, X=0.02, Y=0.02, Z=0.02)
move_joints(e, h, 0.500, 0.500, X=0.01, Y=0.01, Z=0.01)

# XYZ axes, pure linear; joint distance proportional to joint max vel,
# limited to hypot of each joint max sqrt(4.0^2 + 4.1^2 + 4.2^2)
move_joints(e, h, 9.000, 7.103, X=0.040, Y=0.041, Z=0.042)
move_joints(e, h, 7.103, 7.103, X=0.040, Y=0.041, Z=0.042)
move_joints(e, h, 4.000, 4.000, X=0.040, Y=0.041, Z=0.042)

# A-axis only, pure rotary; limited by joint max 90.3
move_joints(e, h, 99.0, 90.3, A=1)
move_joints(e, h, 90.3, 90.3, A=1)
move_joints(e, h, 50.0, 50.0, A=1)

# ABC axes, pure rotary; same distance, limited to hypot of lowest
# joint max sqrt(90.3^2 * 3)
move_joints(e, h, 200.000, 156.404, A=1, B=1, C=1)
move_joints(e, h, 156.404, 156.404, A=1, B=1, C=1)
move_joints(e, h, 100.000, 100.000, A=1, B=1, C=1)

# ABC axes, pure rotary; distance proportional to joint max vel,
# limited to hypot of each joint max sqrt(90.3^2 + 90.4^2 + 90.5^2)
move_joints(e, h, 200.000, 156.577, A=0.903, B=0.904, C=0.905)
move_joints(e, h, 156.577, 156.577, A=0.903, B=0.904, C=0.905)
move_joints(e, h, 100.000, 100.000, A=0.903, B=0.904, C=0.905)

# XA axes, mixed (only linear speed checked):
# - distance proportional to X and A max speeds
move_joints(e, h, 9, 4, X=0.4, A=9.03)
move_joints(e, h, 4, 4, X=0.4, A=9.03)
move_joints(e, h, 1, 1, X=0.4, A=9.03)
# - distance chosen so max speed limited by X max vel 4.0
move_joints(e, h, 9, 4, X=0.4, A=1)
move_joints(e, h, 4, 4, X=0.4, A=1)
move_joints(e, h, 1, 1, X=0.4, A=1)
# - distance chosen so max speed limited by A max vel 90.3
move_joints(e, h, 9, 2, X=0.2, A=9.03)
move_joints(e, h, 2, 2, X=0.2, A=9.03)
move_joints(e, h, 1, 1, X=0.2, A=9.03)

# XYZABC axes, mixed (only linear speed checked): - distance
# - proportional to each joint max speeds; limited to hypot of each
#   joint max sqrt(4.0^2 + 4.1^2 + 4.2^2)
move_joints(e, h, 9.000, 7.103, X=0.40, Y=0.41, Z=0.42, A=9.03, B=9.04, C=9.05)
move_joints(e, h, 7.103, 7.103, X=0.40, Y=0.41, Z=0.42, A=9.03, B=9.04, C=9.05)
move_joints(e, h, 2.000, 2.000, X=0.40, Y=0.41, Z=0.42, A=9.03, B=9.04, C=9.05)
# - distance chosen so max speed limited by hypot of XYZ max vel joint
#   max sqrt(4.0^2 + 4.1^2 + 4.2^2)
move_joints(e, h, 9.000, 7.103, X=0.40, Y=0.41, Z=0.42, A=9, B=9, C=9)
move_joints(e, h, 7.103, 7.103, X=0.40, Y=0.41, Z=0.42, A=9, B=9, C=9)
move_joints(e, h, 4.000, 4.000, X=0.40, Y=0.41, Z=0.42, A=9, B=9, C=9)
# - distance chosen so max speed limited by hypot of ABC max vel joint
#   max
move_joints(e, h, 9.000, 3.551, X=0.200, Y=0.205, Z=0.210, A=9.03, B=9.04, C=9.05)
move_joints(e, h, 3.551, 3.551, X=0.200, Y=0.205, Z=0.210, A=9.03, B=9.04, C=9.05)
move_joints(e, h, 2.000, 2.000, X=0.200, Y=0.205, Z=0.210, A=9.03, B=9.04, C=9.05)

sys.exit(0)

