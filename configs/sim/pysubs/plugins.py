import os
import interpreter
import emccanon

try:
    import emctask
    print "plugins: milltask=",os.getpid()
except ImportError:
    print "plugins: axis=",os.getpid()
    pass


import task
import oword
import remap


