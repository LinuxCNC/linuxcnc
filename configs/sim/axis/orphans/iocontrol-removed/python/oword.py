#   This is a component of LinuxCNC
#   Copyright 2011, 2013 Dewey Garrett <dgarrett@panix.com>, Michael
#   Haberler <git@mah.priv.at>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import sys,os
import interpreter
import emccanon

if 'emctask' in sys.builtin_module_names:
    import emctask

import task

from embedding import *



#define EMC_DEBUG_CONFIG            0x00000002
#define EMC_DEBUG_VERSIONS          0x00000008
#define EMC_DEBUG_TASK_ISSUE        0x00000010
#define EMC_DEBUG_NML               0x00000040
#define EMC_DEBUG_MOTION_TIME       0x00000080
#define EMC_DEBUG_INTERP            0x00000100
#define EMC_DEBUG_RCS               0x00000200
#define EMC_DEBUG_INTERP_LIST       0x00000800
#define EMC_DEBUG_IOCONTROL         0x00001000
#define EMC_DEBUG_OWORD             0x00002000
#define EMC_DEBUG_REMAP             0x00004000
#define EMC_DEBUG_PYTHON            0x00008000
#define EMC_DEBUG_NAMEDPARAM        0x00010000
#define EMC_DEBUG_GDBONSIGNAL       0x00020000

def debug():
    return interpreter.this.debugmask &  0x00008000

# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# len(args) always reflects the number of actual parameters passed
def square(self, *args):
    return args[0]*args[0]


# a function taking a variable number of arguments
# o<multiply> [5] [7]
# (debug, #<_value>)
# o<multiply> [5] [7] [9] [16]
# (debug, #<_value>)
import operator
def multiply(self, *args):
    return reduce(operator.mul, *args)


#----------------  queue calls for task-time execution ------------
# see userfuncs.py for the actual function definitions

# trivial demo: wiggle a user-defined HAL pin a few times
def qdemo(self,*args,**kwargs):
    try:
        task.pytask.enqueue.demo(*args,**kwargs)
        if debug(): print "enqueueing demo()",args,kwargs
    except Exception,e:
        # self happens if called  with the UI context - no task there: harmless
        pass

# access emcStatus
# this is queued so it is done in-sequence at task time
def show_emcstat(self,*args,**kwargs):
    try:
        task.pytask.enqueue.show_emcstat(*args,**kwargs)
        if debug(): print "enqueueing show_emcstat()",args
    except Exception,e:
        if debug(): print "show_emcstat:",e,"pid=",os.getpid()
        pass


def set_named_pin(self,*args):
    ''' an uh, creative way to pass a string argument: use a trailing comment
    usage example:  o<set_named_pin> call [2.345]  (component.pinname)
    '''
    try:
        if (len(args) != 1):
            self.set_errormsg("set_named_pin takes a single argument and a comment")
            return -1
        if len(self.blocks[0].comment) == 0:
            self.set_errormsg("set_named_pin takes  a comment, which is the HAL pin name")
            return -1
        task.pytask.enqueue.set_named_pin(args[0], self.blocks[0].comment)
        if debug(): print "enqueuing set_named_pin()",args
    except Exception,e:
        if debug(): print "set_named_pin:",e,"pid=",os.getpid()
        pass


def  wait_for_named_pin(self,*args):
    ''' same trick to wait for a given named pin to show a certain value:
    usage example:  o<wait_for_named_pin> call [1]  (component.boolpin)

    NB: this will NOT stop readhead, and this is not a method to retrieve a named pin's value
    '''
    try:
        if (len(args) != 1):
            self.set_errormsg("wait_for_named_pin takes a single argument and a comment")
            return -1
        if len(self.blocks[0].comment) == 0:
            self.set_errormsg("wait_for_named_pin takes  a comment, which is the HAL pin name")
            return -1
        task.pytask.enqueue.wait_for_named_pin(args[0], self.blocks[0].comment)
        if debug(): print "enqueuing wait_for_named_pin()",args
    except Exception,e:
        if debug(): print "wait_for_named_pin:",e,"pid=",os.getpid()
        pass

