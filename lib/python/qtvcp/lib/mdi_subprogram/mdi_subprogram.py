#!/usr/bin/env python3
# Qtvcp MDI subprogram
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# This subprogram is used ACTION

import sys
import time
import json
import linuxcnc

from PyQt5.QtCore import QObject
from qtvcp.core import Status, Action, Info

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
STATUS = Status()
ACTION = Action()
INFO = Info()

class MDISubprog(QObject):
    def __init__(self):
        QObject.__init__(self)
        self.send_dict = {'MESSAGE':'finished','CALLER':'Unknown'}
        self.history_log = ""
        self.process()

    def process(self):
        while 1:
            try:
                line = sys.stdin.readline()
            except KeyboardInterrupt:
                break
            if line:
                cmd = line
                line = None
                try:
                    error = self.process_command(cmd)

                    # block polling here 0 main program should start polling in their end
                    STATUS.block_error_polling()

                    # error = 1 means success,
                    # error = None means ignore,
                    # anything else is an error - a returned string is an error message
                    if error is not None:
                        if error != 1:
                            if type(error) == str:
                                sys.stdout.write("ERROR INFO MDI command: {}\n".format(error))
                            else:
                                sys.stdout.write("ERROR MDI routine returned with error\n")
                        else:
                            #self.collect_status()
                            sys.stdout.write("COMPLETE$" + json.dumps(self.send_dict) + "\n")
                            sys.stdout.flush()
                        sys.stdout.flush()

                    # print history
                    if self.history_log != "":
                        time.sleep(0.1)
                        sys.stdout.write("HISTORY$ {}\n".format(self.history_log))
                        self.history_log = ""
                        sys.stdout.flush()

                except Exception as e:
                    sys.stdout.write("ERROR Command Error: {}\n".format(e))
                    sys.stdout.flush()
                break

    # check that the command is actually a method in our class else
    # this message isn't for us - ignore it
    def process_command(self, cmd):
        cmd = cmd.rstrip().split('$')
        if cmd[0] in dir(self):
            if not STATUS.is_on_and_idle(): return None
            pre = self.prechecks()
            if pre is not None: return pre
            parms = json.loads(cmd[1])
            # start polling errors here - parent program should have blocked their polling
            STATUS.unblock_error_polling()
            error = self[cmd[0]](parms)
            if (error != 1 or type(error)== str) and STATUS.is_on_and_idle():
                pass
            self.postreset()
            return error
        else:
            return 'Command function {} not in mdi subprocess routines'.format(cmd[0])

    # for possible future options
    def prechecks(self):
        # return 'ignore'
        return None

    # for possible future options
    def postreset(self):
        return None

    # call each MDI command in this python process
    def mdi_command(self, cmd):
        label = cmd.get('LABEL')
        self.send_dict['CALLER'] = label
        cmdList = cmd.get('COMMANDS')
        timeout = cmd.get('TIMEOUT', 10)
        rtn = self.CALL_MDI_WAIT(cmdList,label,timeout)
        self.history_log = '{} MDI Commands:\n    {}'.format(label,cmdList)
        return rtn

    def CALL_MDI_WAIT(self, code, label, timeout = 10):
        for l in code:
            ACTION.CALL_MDI( l )
            result = ACTION.cmd.wait_complete(timeout)
            try:
                # give a chance for the error message to get to stdin
                time.sleep(.01)
                error = STATUS.ERROR.poll()
                if not error is None:
                    ACTION.ABORT()
                    return '{},\n     {}\n      Called from "{}"'.format(l,error[1],label)
            except Exception as e:
                ACTION.ABORT()
                return '{}'.format(e)
            if result == -1:
                ACTION.ABORT()
                return 'Command timed out: ({} second)'.format(timeout)
            elif result == linuxcnc.RCS_ERROR:
                ACTION.ABORT()
                return 'MDI_COMMAND_WAIT RCS error'
        return 1

########################################
# required boiler code
########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

####################################
# Testing
####################################
if __name__ == "__main__":
    w = MDISubprog()

