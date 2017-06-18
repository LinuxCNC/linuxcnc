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


def introspect(h):
    #print "joint.0.select =", h['joint-0-select']
    #print "joint.0.selected =", h['joint-0-selected']
    #print "joint.0.position =", h['joint-0-position']
    os.system("halcmd show pin halui")
    os.system("halcmd show pin python-ui")
    os.system("halcmd show sig")


def home_joint(name):
    print "    homing", name

    h[name + '-home'] = 1
    start = time.time()
    while (h[name + '-homed'] == 0) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-homed'] == 0:
        print "failed to home", name, "in halui"
        introspect(h)
        sys.exit(1)

    h[name + '-home'] = 0


def select_joint(name, deassert=True):
    print "    selecting", name

    h[name + '-select'] = 1
    start = time.time()
    while (h[name + '-selected'] == 0) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-selected'] == 0:
        print "failed to select", name, "in halui"
        introspect(h)
        sys.exit(1)

    if deassert:
        h[name + '-select'] = 0


def jog_minus(name, target):
    start_position = h[name + '-position']
    print "    jogging", name, "negative: to %.3f" % (target)

    h['jog-selected-minus'] = 1

    start = time.time()
    while (h[name + '-position'] > target) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-position'] > target:
        print name, "failed to jog", name, "to", target
        print "timed out at %.3f after %.3f seconds" % (h[name + '-position'], timeout)
        introspect(h)
        sys.exit(1)

    h['jog-selected-minus'] = 0

    print "    jogged %s negative past target %.3f" % (name, target)

    return True


def jog_plus(name, target):
    start_position = h[name + '-position']
    print "    jogging %s positive: to %.3f" % (name, target)

    h['jog-selected-plus'] = 1

    start = time.time()
    while (h[name + '-position'] < target) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-position'] < target:
        print name, "failed to jog", name, "to", target
        print "timed out at %.3f after %.3f seconds" % (h[name + '-position'], timeout)
        introspect(h)
        sys.exit(1)

    h['jog-selected-plus'] = 0

    print "    jogged %s positive past target %.3f" % (name, target)

    return True


def wait_for_joint_to_stop(joint_number):
    pos_pin = 'joint-%d-position' % joint_number
    vel_pin = 'joint-%d-velocity' % joint_number
    start_time = time.time()
    prev_pos = h[pos_pin]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        new_pos = h[pos_pin]
        if new_pos == prev_pos and h[vel_pin] == 0.0:
            return
        prev_pos = new_pos
    print "Error: joint didn't stop jogging!"
    print "joint %d is at %.3f %.3f seconds after reaching target (prev_pos=%.3f, vel=%.3f)" % (joint_number, h[pos_pin], timeout, prev_pos, h[vel_pin])
    sys.exit(1)


def jog_joint(joint_number, target):
    success = True

    joint = []
    for j in range(0,3):
        joint.append(h['joint-%d-position' % j])

    name = 'joint-%d' % joint_number

    print "jogging", name, "to", target
    select_joint(name)

    if h[name + '-position'] > target:
        jog_minus(name, target)
    else:
        jog_plus(name, target)

    for j in range(0,3):
        pin_name = 'joint-%d-position' % j
        if j == joint_number:
            if joint[j] == h[pin_name]:
                print "joint", str(j), "didn't move but should have!"
                success = False
        else:
            if joint[j] != h[pin_name]:
                print "joint", str(j), "moved from %.3f to %.3f but shouldnt have!" % (joint[j], h[pin_name])
                success = False

    wait_for_joint_to_stop(joint_number)

    if not success:
        sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("python-ui")

h.newpin("joint-0-home", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-homed", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-0-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-0-velocity", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-0-jog-plus", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-jog-minus", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("joint-1-home", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-homed", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-1-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-1-velocity", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-1-jog-plus", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-jog-minus", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("joint-2-home", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-homed", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-2-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-2-velocity", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-2-jog-plus", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-jog-minus", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("jog-selected-minus", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("jog-selected-plus", hal.HAL_BIT, hal.HAL_OUT)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)

# Select joints 1 and 2, but do not de-assert the .select pins.
select_joint('joint-1', deassert=False)
select_joint('joint-2', deassert=False)

# Home all the joints.  This should work fine.
home_joint('joint-0')
home_joint('joint-1')
home_joint('joint-2')

# Deassert the .select pins for joints 1 and 2.
h['joint-1-select'] = 0
h['joint-2-select'] = 0

# The machine is homed and all the .joint.N.select pins are deasserted.
e.set_mode(linuxcnc.MODE_MANUAL)


#
# First some simple single-axis jog & stop.  Test each axis jogging in each
# direction, one at a time.
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong, signalling test failure.
#

jog_joint(0, -0.5)
jog_joint(0, 0.0)

jog_joint(1, -0.5)
jog_joint(1, 0.0)

jog_joint(2, -0.5)
jog_joint(2, 0.0)


#
# Next try selecting a joint and jogging it with the "jog selected" pins,
# then changing which joint is selected.  The expected behavior when
# changing which joint is selected is that the old joint stops jogging and
# the new one starts jogging.
#
# We do this while jogging yet a third joint using its private, per-joint
# jog pins, to verify that it is unaffected by all the drama.
#

name0 = 'joint-0'
name1 = 'joint-1'
name2 = 'joint-2'

start_position0 = h[name0 + '-position']
start_position1 = h[name1 + '-position']
start_position2 = h[name2 + '-position']

print "%s starting at %.3f" % (name0, start_position0)
print "%s starting at %.3f" % (name1, start_position1)
print "%s starting at %.3f" % (name2, start_position2)


#
# start joint0 jogging in the positive direction
#

print "jogging %s positive using the private per-joint jog pins" % name0
h[name0 + '-jog-plus'] = 1

# wait for this joint to come up to speed
start = time.time()
while (h[name0 + '-velocity'] <= 0) and ((time.time() - start) < timeout):
    time.sleep(0.1)
if h[name0 + '-velocity'] <= 0:
    print "%s did not start jogging" % name0
    sys.exit(1)


#
# start the selected joint1 jogging in the negative direction
#

print "jogging selected joint (%s) negative" % (name1)
select_joint(name1)
h['jog-selected-minus'] = 1

# wait for this joint to start moving
start = time.time()
while (h[name1 + '-velocity'] >= 0) and ((time.time() - start) < timeout):
    time.sleep(0.1)
if h[name1 + '-velocity'] >= 0:
    print "%s did not start jogging" % name1
    sys.exit(1)

if h[name0 + '-velocity'] <= 0:
    print "%s stopped jogging" % name0
    sys.exit(1)

if h[name1 + '-position'] >= start_position1:
    print "%s was selected but did not jog negative (start=%.3f, end=%.3f)" % (name1, start_position1, h[name1 + '-position'])
    sys.exit(1)

if h[name2 + '-position'] != start_position2:
    print "%s was not selected but moved (start=%.3f, end=%.3f)" % (name2, start_position2, h[name2 + '-position'])
    sys.exit(1)

print "%s was selected and jogged, %s was not selected and stayed still" % (name1, name2)


start_position1 = h[name1 + '-position']
start_position2 = h[name2 + '-position']

start_velocity1 = h[name1 + '-velocity']
start_velocity2 = h[name2 + '-velocity']

print "selecting %s" % name2
select_joint(name2)

wait_for_joint_to_stop(1)

if h[name0 + '-velocity'] <= 0:
    print "%s stopped jogging" % name0
    sys.exit(1)

if h[name1 + '-velocity'] != 0:
    print "%s was deselected but did not stop (start_vel=%.3f, end_vel=%.3f)" % (name1, start_velocity1, h[name1 + '-velocity'])
    sys.exit(1)

if h[name2 + '-velocity'] >= 0:
    print "%s was selected but did not move (start_vel=%.3f, end_vel=%.3f)" % (name2, start_velocity2, h[name2 + '-velocity'])
    sys.exit(1)

print "%s was deselected and stopped, %s was selected and jogged" % (name1, name2)


start_velocity1 = h[name1 + '-velocity']
start_velocity2 = h[name2 + '-velocity']

print "stopping jog"
h['jog-selected-minus'] = 0

wait_for_joint_to_stop(2)

if h[name0 + '-velocity'] <= 0:
    print "%s stopped jogging" % name0
    sys.exit(1)

if h[name1 + '-velocity'] != 0:
    print "%s started moving again (start_vel=%.3f, end_vel=%.3f)" % (name1, start_velocity1, h[name1 + '-velocity'])
    sys.exit(1)

if h[name2 + '-velocity'] != 0:
    print "%s did not stop (start_vel=%.3f, end_vel=%.3f)" % (name2, start_velocity2, h[name2 + '-velocity'])
    sys.exit(1)

print "%s stopped" % name2


sys.exit(0)

