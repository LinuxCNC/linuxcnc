import remap
import os

def __init__(self):
    # handle any per-module initialisation tasks here
    remap.init_stdglue(self)
    print "interp __init__",self.task,os.getpid()

def __delete__(self):
     # handle any per-module shutdown tasks here
     print "interp __delete__",self.task,os.getpid()
