import sys
import hal
import emccanon
import interpreter

try:
    import emctask
    import customtask
except ImportError,e:
    print "failed on import emctask,customtask",e
    pass

try:
    import cPickle as pickle
except ImportError:
    import pickle

def starttask():
    global pytask
    try:
        import emc
    except ImportError:
        import linuxcnc as emc  # ini only

    ini = emc.ini(emctask.ini_filename())
    t = ini.find("PYTHON", "PYTHON_TASK")
    if int(t) if t else 0:
        pytask = customtask.CustomTask()

if 'emctask' in sys.builtin_module_names:
    starttask()
