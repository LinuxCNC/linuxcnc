#!/usr/bin/env python
"""Copied from m61-test"""

import linuxcnc
import os
import re
from time import sleep


class LinuxcncError(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)


class LinuxcncControl:
    """
    issue G-Code commands
    make sure important modes are saved and restored
    mode is saved only once, and can be restored only once

    usage example:
        e = emc_control()
        e.prepare_for_mdi()
            any internal sub using e.g("G0.....")
        e.finish_mdi()

    """

    def __init__(self, timeout=2, throw_exceptions=False):
        self.c = None
        self.e = None
        self.s = None
        self.timeout = timeout
        self.error_list = []
        self.raise_on_error = throw_exceptions
        # Ignore blank errors by default
        self.err_ignore_mask = '^$'
        self.try_init(ignore_error=True)

    def try_init(self, ignore_error=False):
        try:
            self.c = linuxcnc.command()
            self.e = linuxcnc.error_channel()
            self.s = linuxcnc.stat()
            self.s.poll()
        except linuxcnc.error as e:
            if ignore_error:
                return False

        return True

    def wait_for_backend(self, timeout_sec):
        k = 0
        print('Waiting for LinuxCNC...')
        while k < 25:
            k += 1
            sleep(1)
            if self.try_init(ignore_error=True):
                return True

        return False

    def running(self, do_poll=True):
        """
        check if interpreter is running.
        If so, cant switch to MDI mode.
        """
        if do_poll:
            self.s.poll()
        return (self.s.task_mode == linuxcnc.MODE_AUTO and
                self.s.interp_state != linuxcnc.INTERP_IDLE)

    def set_mode(self, m):
        """
        set EMC mode if possible, else throw LinuxcncError
        return current mode
        """
        self.s.poll()
        if self.s.task_mode == m :
            return m
        if self.running(do_poll=False):
            raise LinuxcncError("interpreter running - cant change mode")
        self.c.mode(m)
        self.c.wait_complete()

        return m

    def set_state(self, m):
        """
        set EMC mode if possible, else throw LinuxcncError
        return current mode
        """
        self.s.poll()
        if self.s.task_mode == m :
            return m
        self.c.state(m)
        self.c.wait_complete(self.timeout)
        return m

    def do_home(self, axismask):
        self.s.poll()
        self.c.home(axismask)
        self.c.wait_complete(self.timeout)

    def ok_for_mdi(self):
        """
        check wether ok to run MDI commands.
        """
        self.s.poll()
        return not self.s.estop and self.s.enabled and self.s.homed

    def prepare_for_mdi(self):
        """
        check wether ok to run MDI commands.
        throw  LinuxcncError if told so.
        return current mode
        """

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

    def g(self, code, wait=False):
        """
        issue G-Code as MDI command.
        wait for completion if reqested
        """

        self.c.mdi(code)
        if wait:
            try:
                while self.c.wait_complete(self.timeout) == -1:
                    pass
                return True
            except KeyboardInterrupt:
                self.error_list.append("warning: interrupted by keyboard in c.wait_complete(self.timeout)")
                return False

        self.poll_and_log_error(code)
        return False

    def get_current_tool(self):
        self.s.poll()
        return self.s.tool_in_spindle

    def active_codes(self):
        self.s.poll()
        return self.s.gcodes

    def get_current_system(self):
        g = self.active_codes()
        for i in g:
                if i >= 540 and i <= 590:
                        return i/10 - 53
                elif i >= 590 and i <= 593:
                        return i - 584
        return 1

    def open_program(self, filename):
        """Open an nc file"""
        self.s.poll()
        self.set_mode(linuxcnc.MODE_AUTO)
        self.c.wait_complete()
        sleep(.25)
        full_file = os.path.join(os.getcwd(), filename)
        self.c.program_open(full_file)
        self.c.wait_complete()

    def run_full_program(self):
        """Start a loaded program"""
        self.s.poll()
        self.c.auto(linuxcnc.AUTO_RUN, 0)
        return self.c.wait_complete(self.timeout)

    def set_feed_scale(self, scale):
        """Assign a feed scale"""

        self.s.poll()
        self.c.feedrate(scale)
        self.c.wait_complete(self.timeout)

    def wait_on_program(self, f):
        self.s.poll()
        while self.s.exec_state != linuxcnc.EXEC_DONE or self.s.state != linuxcnc.RCS_DONE and self.s.task_state == linuxcnc.STATE_ON:
            sleep(.01)
            self.s.poll()
            self.flush_errors(f)
            if self.s.task_state != linuxcnc.STATE_ON:
                self.error_list.append('{}: error: motion disabled'.format(f))
                return False

        return True

    def run_until_done(self, f):
        self.set_mode(linuxcnc.MODE_AUTO)
        self.open_program(f)
        self.set_mode(linuxcnc.MODE_AUTO)
        self.run_full_program()
        return self.wait_on_program(f)

    def check_rcs_error(self):
        self.s.poll()
        if self.s.state == linuxcnc.RCS_ERROR:
            return True
        return False

    def set_error_ignore_pattern(self, pattern):
        self.err_ignore_mask = pattern

    def poll_and_log_error(self, f):
        error = self.e.poll()
        if error is None:
            return False

        err_type, msg_text = error
        err_type_str = "error" if err_type in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR) else "info"
        err_str = "{}: {}: {}".format(f, err_type_str, msg_text)
        self.error_list.append(err_str)
        if self.raise_on_error:
            raise LinuxcncError(err_str)
        return True

    def flush_errors(self, filename=""):
        while self.poll_and_log_error(filename):
            sleep(0.001)
            continue

    def has_error(self):
        for msg in self.error_list:
            if re.search(self.err_ignore_mask, msg):
                continue
            if re.search('error: ', msg):
                return True
        else:
            return False
