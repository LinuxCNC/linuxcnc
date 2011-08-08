import sys
import hal
import emccanon
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


# instantiate Python Io Task()
if 'emctask' in sys.builtin_module_names:
    pytask = customtask.CustomTask()

