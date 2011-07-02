import task
import InterpMod

if InterpMod.under_task:
    import TaskMod


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
    except Exception:
        pass

def notify(args):
    try:
        c = InterpMod.interp.blocks[0].comment
        print "in oword.notify() comment=",c
        task.enqueue.notify(c)
    except Exception,e:
        print "notify: ",e
        pass

# access emcStatus
def tdir(args):
    if InterpMod.under_task:
        e = TaskMod.EMC_STAT
        print "mode=",e.task.mode
        print "state=",e.task.state
        print "file=",e.task.file
        print "toolOffset=",e.task.toolOffset
