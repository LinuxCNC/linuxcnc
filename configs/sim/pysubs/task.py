import sys
import hal
import canon
import interpreter

try:
    import emctask
    import customtask
    print "-- emctask & customtask imported"
except ImportError:
    pass

try:
    import cPickle as pickle
except ImportError:
    import pickle


def debug():
    return interpreter.this.debugmask &  0x00008000 #  EMC_DEBUG_PYTHON


# this is called by task once after startup
# task_init() is NOT called when running in the UI
def task_init():
    global h
    if debug(): print "Python: task_init() called"

    h = hal.component("iocontrol-task")
    h.newpin("coolant-flood", hal.HAL_BIT, hal.HAL_OUT)
    h.newpin("coolant-mist", hal.HAL_BIT, hal.HAL_OUT)

    h.newpin("lube-level", hal.HAL_BIT, hal.HAL_OUT)
    h.newpin("lube", hal.HAL_BIT, hal.HAL_OUT)

    h.newpin("emc-enable-in", hal.HAL_BIT, hal.HAL_IN)
    h.newpin("user-enable-out", hal.HAL_BIT, hal.HAL_OUT)
    h.newpin("user-request-enable", hal.HAL_BIT, hal.HAL_OUT)

    h.newpin("tool-changed", hal.HAL_BIT, hal.HAL_IN)
    h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_OUT)
    h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_OUT)
    h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_OUT)
    h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_IN)

    h.newpin("bit", hal.HAL_BIT, hal.HAL_OUT)

    h.newpin("float", hal.HAL_FLOAT, hal.HAL_OUT)
    h.newpin("int", hal.HAL_S32, hal.HAL_OUT)
    h.ready()

    return 1

#----------------  exec-time plugin support ----------

# methods in this class will be executed task-time if
# queued like in the oword.task example
class Execute(object):

    def demo(self,s):
        global h
        if debug(): print "TASK: demo(%s)" % s
        try:
            for i in range(int(s[0])):
                h['bit'] = not  h['bit']
        except Exception,e:
            print "bad:",e
            pass
        return 0
    # return -1 to fail execution
    #interpreter.this.set_errormsg("really bad")
    #return -1

    def set_named_pin(self,value,name):
        global h
        if debug(): print "set_named_pin ",value,name
        if type(h[name]).__name__ == 'float':
            h[name] = value

        if type(h[name]).__name__ == 'int':
            h[name] = int(value)

        if type(h[name]).__name__ == 'bool':
            h[name] = bool(int(value))
        return 0

    def notify(self, s):
        print "notify s=",s
        return 0


# this function is called at execution time if a EMC_EXEC_PLUGIN_CALL
# NML message is encountered by task
# unwrap method name and arguments, and call the method
def plugin_call(s):
    global execute
    call = pickle.loads(s)
    func = getattr(execute, call[0], None)
    if func:
	    return func(*call[1],**call[2])
    else:
        raise AttributeError, "no such method: " + call[0]

# support queuing calls to Python methods:
# trap call, pickle a tuple of name and arguments and enqueue with canon PLUGIN_CALL
class EnqueueCall(object):
    def __init__(self,e):
        self._e = e

    def _encode(self,*args,**kwargs):
        if hasattr(self._e,self._name) and callable(getattr(self._e,self._name)):
            p = pickle.dumps((self._name,args,kwargs)) # ,-1) # hm, binary wont work just yet
            canon.PLUGIN_CALL(int(len(p)),p)
        else:
            raise AttributeError,"no such method: " + self._name

    def __getattr__(self, name):
        self._name = name
        return self._encode

execute = Execute()
enqueue = EnqueueCall(execute)


