import os
import interpreter
import emccanon
have_emctask = False
try:
    import emctask
    have_task = True
    print "plugins: milltask=",os.getpid()
except ImportError:
    print "plugins: axis=",os.getpid()
    pass


import task
import oword
import remap


print "plugins MAIN"


