import sys
import traceback
from math import sin,cos

from interpreter import *
from emccanon import MESSAGE, SET_MOTION_OUTPUT_BIT, CLEAR_MOTION_OUTPUT_BIT,SET_AUX_OUTPUT_BIT,CLEAR_AUX_OUTPUT_BIT

from util import lineno, call_pydevd

throw_exceptions = 1 # raises InterpreterException if execute() or read() fail

def g886(self, **words):
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    if words.has_key('p'):
        MESSAGE("the P word was present")
    MESSAGE("comment on this line: '%s'" % (self.blocks[self.remap_level].comment))
    return INTERP_OK


def involute(self, **words):
    """ remap function with raw access to Interpreter internals """

    if self.debugmask & 0x20000000: call_pydevd() # USER2 debug flag

    if equal(self.feed_rate,0.0):
        self.set_errormsg("feedrate > 0 required")
        return INTERP_ERROR

    if equal(self.speed,0.0):
        self.set_errormsg("spindle speed > 0 required")
        return INTERP_ERROR

    plunge = 0.1 # if Z word was given, plunge - with reduced feed

    # inspect controlling block for relevant words
    c = self.blocks[self.remap_level]
    x0 = c.x_number if c.x_flag else 0
    y0 = c.y_number if c.y_flag else 0
    a  = c.p_number if c.p_flag else 10
    old_z = self.current_z

    if self.debugmask & 0x10000000:   # USER1 debug flag
        print "x0=%f y0=%f a=%f old_z=%f" % (x0,y0,a,old_z)

    try:
        #self.execute("G3456")  # would raise InterpreterException
        self.execute("G21",lineno())
        self.execute("G64 P0.001",lineno())
        self.execute("G0 X%f Y%f" % (x0,y0),lineno())

        if c.z_flag:
            feed = self.feed_rate
            self.execute("F%f G1 Z%f" % (feed * plunge, c.z_number),lineno())
            self.execute("F%f" % (feed),lineno())

        for i in range(100):
            t = i/10.
            x = x0 + a * (cos(t) + t * sin(t))
            y = y0 + a * (sin(t) - t * cos(t))
            self.execute("G1 X%f Y%f" % (x,y),lineno())

        if c.z_flag: # retract to starting height
            self.execute("G0 Z%f" % (old_z),lineno())

    except InterpreterException,e:
        msg = "%d: '%s' - %s" % (e.line_number,e.line_text, e.error_message)
        self.set_errormsg(msg) # replace builtin error message
        return INTERP_ERROR

    return INTERP_OK


def m462(self, **words):
    """ remap function which does the equivalent of M62, but via Python """

    p = int(words['p'])
    q = int(words['q'])

    if q:
        SET_MOTION_OUTPUT_BIT(p)
    else:
        CLEAR_MOTION_OUTPUT_BIT(p)
    return INTERP_OK

def m465(self, **words):
    """ remap function which does the equivalent of M65, but via Python """

    p = int(words['p'])
    q = int(words['q'])

    if q:
        SET_AUX_OUTPUT_BIT(p)
    else:
        CLEAR_AUX_OUTPUT_BIT(p)
    return INTERP_OK



def g280(self, **words):
    MESSAGE("g280:")
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    return INTERP_OK

def g281(self, **words):
    MESSAGE("g281:")
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    return INTERP_OK

def g300(self, **words):
    MESSAGE("g300:")
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    return INTERP_OK

def g301(self, **words):
    MESSAGE("g301:")
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    return INTERP_OK
