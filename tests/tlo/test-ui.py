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


def introspect():
    os.system("halcmd show pin python-ui")


def wait_for_pin_value(pin_name, value, timeout=1):
    print "waiting for %s to go to %f (timeout=%f)" % (pin_name, value, timeout)

    start = time.time()
    while (h[pin_name] != value) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[pin_name] != value:
        print "timeout!  pin %s didn't get to %f" % (pin_name, value)
        introspect()
        sys.exit(1)

    print "pin %s went to %f!" % (pin_name, value)


def verify_pin_value(pin_name, value):
    if (h[pin_name] != value):
        print "pin %s is %f, expected %f" % (pin_name, h[pin_name], value)
        sys.exit(1);

    print "pin %s is %f" % (pin_name, value)


def get_interp_param(param_number):
    e.c.mdi("(debug, #%d)" % param_number)
    while e.c.wait_complete() == -1:
        pass

    # wait up to 2 seconds for a reply
    start = time.time()
    while (time.time() - start) < 2:
        error = e.e.poll()
        if error == None:
            time.sleep(0.010)
            continue

        kind, text = error
        if kind == linuxcnc.OPERATOR_DISPLAY:
            return float(text)

        print text

    print "error getting parameter %d" % param_number
    return None


def verify_interp_param(param_number, expected_value):
    param_value = get_interp_param(param_number)
    print "interp param #%d = %f (expecting %f)" % (param_number, param_value, expected_value)
    if math.fabs(param_value - expected_value) > 0.000001:
        print "ERROR: interp param #%d = %f, expected %f" % (param_number, param_value, expected_value)
        sys.exit(1)


def verify_stable_pin_values(pins, duration=1):
    start = time.time()
    while (time.time() - start) < duration:
        for pin_name in pins.keys():
            val = h[pin_name]
            if val != pins[pin_name]:
                print "ERROR: pin %s = %f (expected %f)" % (pin_name, val, pin[pin_name])
                sys.exit(1)
        time.sleep(0.010)


def verify_tool(tool, x, y, z, a, b, c, u, v, w, diameter, front_angle, back_angle):
    wait_for_pin_value('tool-number', tool)
    verify_interp_param(5400, tool)

    # verify the current location, as offset by TLO
    verify_interp_param(5420, x)
    verify_interp_param(5421, y)
    verify_interp_param(5422, z)
    verify_interp_param(5423, a)
    verify_interp_param(5424, b)
    verify_interp_param(5425, c)
    verify_interp_param(5426, u)
    verify_interp_param(5427, v)
    verify_interp_param(5428, w)

    # verify tool diameter & angles
    verify_interp_param(5410, diameter)
    verify_interp_param(5411, front_angle)
    verify_interp_param(5412, back_angle)

    return True




#
# connect to HAL
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("python-ui")

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.g_raise_except = False
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.set_mode(linuxcnc.MODE_MDI)


#
# this is a non-random toolchanger, so it starts with no tool in the spindle
# make sure the startup condition is sane
#

print "*** starting up, expecting T0 in spindle & no TLO"

verify_tool(
    tool=0,
    x=0, y=0, z=0,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0,
    front_angle=0,
    back_angle=0
)


print "*** load T100 but dont apply TLO"

e.g('t100 m6')

verify_tool(
    tool=100,
    x=0, y=0, z=0,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print "*** apply TLO of loaded tool (T100)"

e.g('g43')

verify_tool(
    tool=100,
    x=-2, y=0, z=-1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print "*** apply TLO of T200 instead"

e.g('g43 h200')

verify_tool(
    tool=100,
    x=-0.2, y=0, z=-0.1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print "*** try to add in TLO with no H-word, should fail"

# first drain the error queue
start = time.time()
while (time.time() - start) < 2:
    error = e.e.poll()
    if error == None:
        # no more queued errors, continue with test
        break

e.g('g43.2')
if e.error[1] != "G43.2: H-word missing":
    print "G43.2 with missing H-word did not produce expected error"
    print "got [%s]" % e.error[1]
    sys.exit(1)


print "*** add in TLO of T100"

e.g('g43.2 h100')

verify_tool(
    tool=100,
    x=-2.2, y=0, z=-1.1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print "*** add in TLO of T300"

e.g('g43.2 h300')

verify_tool(
    tool=100,
    x=-2.22, y=0, z=-1.11,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print "*** add in TLO of T400"

e.g('g43.2 h400')

verify_tool(
    tool=100,
    x=-2.222, y=0, z=-1.111,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print "*** add in TLO of T400 again"

e.g('g43.2 h400')

verify_tool(
    tool=100,
    x=-2.224, y=0, z=-1.112,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print "*** now let's try it rotated.  first, just rotate but don't change the tlo"

e.g('g10 l2 p1 r33')

verify_tool(
    tool=100,
    x=-2.224 * math.cos(math.radians(33)), y=2.224 * math.sin(math.radians(33)), z=-1.112,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print "*** clear tlo"

e.g('g49')

verify_tool(
    tool=100,
    x=-0, y=0, z=0,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print "*** apply t100"

e.g('g43 h100')

verify_tool(
    tool=100,
    x=-2*math.cos(math.radians(33)), y=2*math.sin(math.radians(33)), z=-1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print "*** add in t200"

e.g('g43.2 h200')

verify_tool(
    tool=100,
    x=-2.2*math.cos(math.radians(33)), y=2.2*math.sin(math.radians(33)), z=-1.1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)



# if we get here it all worked!
sys.exit(0)

