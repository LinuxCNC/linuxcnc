import sys
import interpreter

have_emctask = False
if 'emctask' in sys.builtin_module_names:
    import emctask
    have_task = True

import task

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
    try:
        task.enqueue.demo(args)
    except Exception,e:
        print "uh,oh",e
        pass

def notify(args):
    try:
        c = interpreter.this.blocks[0].comment
        print "in oword.notify() comment=",c
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
