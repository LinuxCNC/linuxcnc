import sys,os
import interpreter
import emccanon

if 'emctask' in sys.builtin_module_names:
    import emctask

import task

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
def square(args):
    return args[0]*args[0]


#----------------  queue calls for task-time execution ------------
# see userfuncs.py for the actual function definitions

# trivial demo: wiggle a user-defined HAL pin a few times
def qdemo(args):
    try:
        task.pytask.enqueue.demo(args)
        if debug(): print "enqueueing demo()",args
    except Exception,e:
        # this happens if called  with the UI context - no task there
        if debug(): print "qdemo:",e,"pid=",os.getpid()
        pass

# access emcStatus
# this is queued so it is done in-sequence at task time
def show_emcstat(args):
    try:
        task.pytask.enqueue.show_emcstat(args)
        if debug(): print "enqueueing show_emcstat()",args
    except Exception,e:
        if debug(): print "show_emcstat:",e,"pid=",os.getpid()
        pass


def set_named_pin(args):
    ''' an uh, creative way to pass a string argument: use a trailing comment
    usage example:  o<set_named_pin> call [2.345]  (component.pinname)
    '''
    try:
        if (len(args) != 1):
            interpreter.this.set_errormsg("set_named_pin takes a single argument and a comment")
            return -1
        if len(interpreter.this.blocks[0].comment) == 0:
            interpreter.this.set_errormsg("set_named_pin takes  a comment, which is the HAL pin name")
            return -1
        task.pytask.enqueue.set_named_pin(args[0], interpreter.this.blocks[0].comment)
        if debug(): print "enqueuing set_named_pin()",args
    except Exception,e:
        if debug(): print "set_named_pin:",e,"pid=",os.getpid()
        pass


def  wait_for_named_pin(args):
    ''' same trick to wait for a given named pin to show a certain value:
    usage example:  o<wait_for_named_pin> call [1]  (component.boolpin)
    '''
    try:
        if (len(args) != 1):
            interpreter.this.set_errormsg("wait_for_named_pin takes a single argument and a comment")
            return -1
        if len(interpreter.this.blocks[0].comment) == 0:
            interpreter.this.set_errormsg("wait_for_named_pin takes  a comment, which is the HAL pin name")
            return -1
        task.pytask.enqueue.wait_for_named_pin(args[0], interpreter.this.blocks[0].comment)
        if debug(): print "enqueuing wait_for_named_pin()",args
    except Exception,e:
        if debug(): print "wait_for_named_pin:",e,"pid=",os.getpid()
        pass

