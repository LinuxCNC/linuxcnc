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


def select_joint(name):
    print "    selecting", name

    h[name + '-select'] = 1
    start = time.time()
    while (h[name + '-selected'] == 0) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-selected'] == 0:
        print "failed to select", name, "in halui"
        introspect(h)
        sys.exit(1)

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
        introspect(h)
        return False

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
        introspect(h)
        return False

    h['jog-selected-plus'] = 0

    print "    jogged %s positive past target %.3f)" % (name, target)

    return True


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


    # give the joint time to stop
    # FIXME: close the loop here
    time.sleep(0.1)

    if not success:
        sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("python-ui")

h.newpin("joint-0-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-1-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-2-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)

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
e.set_mode(linuxcnc.MODE_MANUAL)


#
# run the test
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong.
#

jog_joint(0, -0.5)
jog_joint(0, 0.0)

jog_joint(1, -0.5)
jog_joint(1, 0.0)

jog_joint(2, -0.5)
jog_joint(2, 0.0)


sys.exit(0)

