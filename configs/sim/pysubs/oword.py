import sys
import interpreter


have_emctask = False
if 'emctask' in sys.builtin_module_names:
    import emctask
    have_task = True

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
# len(args) reflects the number of actual parameters passed
def square(args):
    return args[0]*args[0]


#----------------  queue a call for task-time execution ------------
def qdemo(args):
    print "in oword.qdemo() - enqueueing demo",args
    task.enqueue.demo(args)


def set_named_pin(args):

    if (len(args) != 1):
        interpreter.this.set_errormsg("set_named_pin takes a single argument and a comment")
    if len(interpreter.this.blocks[0].comment) == 0:
            interpreter.this.set_errormsg("set_named_pin takes  a comment, which is the HAL pin name")

    task.enqueue.set_named_pin(args[0], interpreter.this.blocks[0].comment)

def notify(args):
    try:
        c = interpreter.this.blocks[0].comment
        if debug(): print "in oword.notify() comment=",c
        task.enqueue.notify(c)
    except Exception,e:
        print "notify: ",e
        pass

# access emcStatus
def tdir(args):
    if have_emctask:
        e = emctask.EMC_STAT
        print "mode=",e.task.mode
        print "state=",e.task.state
        print "file=",e.task.file
        print "toolOffset=",e.task.toolOffset
