from interpreter import INTERP_OK
from emccanon import MESSAGE,STRAIGHT_FEED,STRAIGHT_TRAVERSE
from math import sin,cos
import inspect

def g886(self, userdata, **words):
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    if words.has_key('p'):
        MESSAGE("the P word was present")
    MESSAGE("comment on this line: '%s'" % (self.blocks[self.remap_level].comment))
    return INTERP_OK

# self.params["_[config]xpos"],self.params["_[config]ypos"],self.params["_[config]zpos"],

def involute(self, userdata, **words):
    x0 = words['x'] if words.has_key('x') else 0.0  # center
    y0 = words['y'] if words.has_key('y') else 0.0
    z = words['z']  # plunge level
    p = words['p'] if words.has_key('q') else 0.1   # Z safety height
    a = words['q'] if words.has_key('q') else 0.1
    print "feed=", self.feed_rate
    #print "G20 F60"
    #print "G64 P0.001" G64 Best Possible Speed
    #print "G0 X0 Y0 Z0"
    STRAIGHT_TRAVERSE(lineno(),x0,y0,z + p,0,0,0,0,0,0) # rapid at safety height
    STRAIGHT_FEED(lineno(),x0,y0,z,0,0,0,0,0,0) # plunge
    for i in range(100):
        t = i/10.
        x = x0 + a * (cos(t) + t * sin(t))
        y = y0 + a * (sin(t) - t * cos(t))
        STRAIGHT_FEED(lineno(),x,y,z,0,0,0,0,0,0)
    return INTERP_OK

## {{{ http://code.activestate.com/recipes/145297/ (r1)
def lineno():
    """Returns the current line number in this script."""
    return inspect.currentframe().f_back.f_lineno
