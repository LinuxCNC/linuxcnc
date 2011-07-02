import hal
import CanonMod
import InterpMod
import pickle

h = None

# this is called by task once after startup
def task_init():
	global h
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
	h.ready()
	return 1



#----------------  exec-time plugin support ----------

# this function is called at execution time if a EMC_EXEC_PLUGIN_CALL
# NML message is encountered
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
        if hasattr(self._e,self._name):# and callable(getattr(self.e,self.name)):
            CanonMod.PLUGIN_CALL(pickle.dumps((self._name,args,kwargs)))
        else:
            raise AttributeError,"no such method: " + self._name

    def __getattr__(self, name):
        self._name = name
        return self._encode

# methods in this class will be executed task-time if
# queued like in the oword.task example
class Execute(object):

    def demo(self,s):
	    global h
	    if h: # running under interp
		    print "TASK: demo(%s)" % s
		    for i in range(int(s[0])):
			    h['bit'] = not  h['bit']
            return 0
            # return -1 to fail execution
            #InterpMod.interp.set_errormsg("FoooooooO!!")
	    #return -1

execute = Execute()
enqueue = EnqueueCall(execute)
