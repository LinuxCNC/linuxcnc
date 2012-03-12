from interpreter import *
from emccanon import MESSAGE

from stdglue import cycle_prolog, cycle_epilog, init_stdglue

# This shows how to create a remapped G code  which can be used as a cycle
# written in Python
#
# Example:
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
# for a more through example of a cycle prolog, see cycle_prolog in stdglue.py

_sticky_params = dict()

def g842(self,**words):
 
    global _sticky_params

    firstcall = False
    
    # determine whether this is the first or a subsequent call
    c = self.blocks[self.remap_level]
    r = c.executing_remap
    if c.g_modes[1] == r.motion_code:
        # this was the first call.
        # clear the dict to remember all sticky parameters.
        _sticky_params[r.name] = dict()
        text = "*" + r.name
    else:
        text = r.name
    # merge in new parameters
    _sticky_params[r.name].update(words) 

    # insert your cycle actions here
    for (key,value) in _sticky_params[r.name].items():
        text += "%s%.1f " % (key, value)
    MESSAGE(text)

    # retain the current motion mode
    self.motion_mode = c.executing_remap.motion_code 
    return INTERP_OK

