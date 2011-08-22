import sys
import inspect
import traceback
from math import sin,cos

from interpreter import *
from emccanon import MESSAGE

from toolchange import prepare_prolog, prepare_epilog, change_prolog, change_epilog, set_tool_number

def g886(self, userdata, **words):
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    if words.has_key('p'):
        MESSAGE("the P word was present")
    MESSAGE("comment on this line: '%s'" % (self.blocks[self.remap_level].comment))
    return INTERP_OK


def involute(self, userdata, **words):
    """ remap function with raw access to Interpreter internals """

    if equal(self.feed_rate,0.0):
        self.set_errormsg("feedrate > 0 required")
        return INTERP_ERROR

    if equal(self.speed,0.0):
        self.set_errormsg("spindle speed > 0 required")
        return INTERP_ERROR

    plunge = 0.1 # if Z word was given, plunge - with reduced feed

    # inspect controlling block for relevant words
    cblock = self.blocks[self.remap_level]
    x0 = cblock.x_number if cblock.x_flag else 0
    y0 = cblock.y_number if cblock.y_flag else 0
    a  = cblock.p_number if cblock.p_flag else 10
    old_z = self.current_z

    print "x0=%f y0=%f a=%f old_z=%f" % (x0,y0,a,old_z)

    try:
        #self.execute("G3456") # raises InterpreterException
        self.execute("G21",lineno())
        self.execute("G64 P0.001",lineno())
        self.execute("G0 X%f Y%f" % (x0,y0),lineno())

        if cblock.z_flag:
            feed = self.feed_rate
            self.execute("F%f G1 Z%f" % (feed * plunge, cblock.z_number),lineno())
            self.execute("F%f" % (feed),lineno())

        for i in range(100):
            t = i/10.
            x = x0 + a * (cos(t) + t * sin(t))
            y = y0 + a * (sin(t) - t * cos(t))
            self.execute("G1 X%f Y%f" % (x,y),lineno())

        if cblock.z_flag: # retract to starting height
            self.execute("G0 Z%f" % (old_z),lineno())

    except InterpreterException,e:
        msg = "%d: '%s' - %s" % (e.line_number,e.line_text, e.error_message)
        self.set_errormsg(msg) # replace builtin error message
        return INTERP_ERROR

    return INTERP_OK

def lineno():
    return inspect.currentframe().f_back.f_lineno
