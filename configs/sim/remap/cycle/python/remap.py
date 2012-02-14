from interpreter import *
from emccanon import MESSAGE

# This shows how to create a remapped G code  which can be used as a cycle

# Assume G84.2 is remapped to Python g842 like so in the [RS274NGC] ini section:
# REMAP=G84.2 argspec=xyzqp python=g842 modalgroup=1
#    
# then executing
#   
#    G84.2 x1 y1 (line1)
#    x3 y3       (line2)
#    y5          (line3)
#    ...
#
#  will execute like:
#   *G84.2 x1 y1
#    G84.2 x3 y3
#    G84.2 x3 y5
#    
# until motion is cleared with G80 or some other motion is executed.
#   
# This enables writing cycles in Python, or as Oword procedures; in the
# latter case the self.motion_mode should be set in the Python epilog.
# 

g842_sticky = dict() 
g843_sticky = dict() 

def g842(self,**words):
    
    global g842_sticky

    firstcall = False
    
    # determine whether this is the first or a subsequent call
    c = self.blocks[self.remap_level]
    if c.g_modes[1] == 842:
        # this was the first call.
        # clear the dict to remember all sticky parameters.
        g842_sticky = dict()
        firstcall = True

    g842_sticky.update(words) # merge in new parameters

    text = "*G84.2 " if firstcall else "G84.2 "
    for (key,value) in g842_sticky.items():
        text += "%s%.1f " % (key, value)
    MESSAGE(text)

    self.motion_mode = 842 # retain the current motion mode
    return INTERP_OK


# a cycle with an Oword sub 
#REMAP=G84.3  modalgroup=1 argspec=xyzqp prolog=g843_prolog ngc=g843 epilog=g843_epilog

# not much different than above, except that we explicitly export the names
# to the ngc procedure:

def g843_prolog(self,**words):
    global g843_sticky
    firstcall = False
    
    # determine whether this is the first or a subsequent call
    c = self.blocks[self.remap_level]
    if c.g_modes[1] == 843:
        # this was the first call.
        # clear the dict to remember all sticky parameters.
        g843_sticky = dict()
        firstcall = True

    g843_sticky.update(words) # merge in new parameters

    for (key,value) in g843_sticky.items():
        self.params[key] = value

    return INTERP_OK


def g843_epilog(self,**words):
    self.motion_mode = 843 # retain the current motion mode
    return INTERP_OK
