#!/usr/bin/env python
'''Copied from m61-test'''

import linuxcnc
import hal

from time import sleep
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

    def open_program(self,filename):
        '''Open an nc file'''
        self.s.poll()
        self.c.program_open(filename)
        self.c.wait_complete()

    def run_full_program(self):
        '''Start a loaded program'''
        self.s.poll()
        self.c.auto(linuxcnc.AUTO_RUN, 0)
        self.c.wait_complete()

    def set_feed_scale(self,scale):
        '''Assign a feed scale'''
        pass
#TODO low-level control commands?

def introspect():
    os.system("halcmd show pin python-ui")

if __name__ == '__main__':
    """Run the test"""


    h = hal.component("python-ui")
    h.ready() # mark the component as 'ready'

    os.system("halcmd source ./postgui.hal")

    #
    # connect to LinuxCNC
    #

    e = LinuxcncControl()
    e.g_raise_except = False
    e.set_state(linuxcnc.STATE_ESTOP_RESET)
    sleep(1)
    e.set_state(linuxcnc.STATE_ON)
    sleep(1)
    e.set_mode(linuxcnc.MODE_MANUAL)
    sleep(1)

    e.do_home(-1)
    sleep(.5)

    e.set_mode(linuxcnc.MODE_AUTO)
    sleep(.5)
    e.open_program(sys.argv[1])
    sleep(.5)
    e.run_full_program()
    sleep(.5)


    # if we get here it all worked!
    sys.exit(0)

