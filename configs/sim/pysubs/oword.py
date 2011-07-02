import task

#----------------  queue a call for task-time execution ------------
def qdemo(args):
    print "in oword.qdemo() - enqueueing demo",args
    try:
        task.enqueue.demo(args)
    except Exception,e:
        print "Exception",e

# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# len(args) reflects the number of actual parameters passed
def square(args):
	return args[0]*args[0]
