import os
import interpreter
import emccanon

try:
    import emctask
    print "toplevel: milltask=",os.getpid()
except ImportError:
    print "toplevel: axis=",os.getpid()
    pass


import task
import oword
import remap


