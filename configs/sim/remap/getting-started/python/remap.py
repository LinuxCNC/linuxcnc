import sys
import inspect
import traceback
from interpreter import *
from emccanon import MESSAGE,STRAIGHT_FEED,STRAIGHT_TRAVERSE,enqueue_SET_FEED_RATE
from math import sin,cos


def g886(self, userdata, **words):
    for key in words:
        MESSAGE("word '%s' = %f" % (key, words[key]))
    if words.has_key('p'):
        MESSAGE("the P word was present")
    MESSAGE("comment on this line: '%s'" % (self.blocks[self.remap_level].comment))
    return INTERP_OK


def error_stack(self):
    print "error stack level=%d" % (self.stack_index)
    for s in self.stack():
        print "--'%s'--" % (s)


def lineno():
    """Returns the current line number in this script."""
    return inspect.currentframe().f_back.f_lineno

def callstack(self):
    for i in range(self.call_level):
        c = self.sub_context[i]
        print "%d: pos=%d seq=%d filename=%s sub=%s" % (i,c.position, c.sequence_number,c.filename,c.subname)

def involute(self, userdata, **words):
    """ remap function with raw access to Interpreter internals """

    cblock = self.blocks[self.remap_level]
    print "called from %s:%d" % (self.filename,cblock.line_number)
    callstack(self)

    if equal(self.feed_rate,0.0):
        self.set_errormsg("feedrate > 0 required")
        return INTERP_ERROR

    if equal(self.speed,0.0):
        self.set_errormsg("spindle speed > 0 required")
        return INTERP_ERROR

    plunge = 0.3 # if Z word was given, plunge with reduced feed

    x0 = cblock.x_number if cblock.x_flag else 0
    y0 = cblock.y_number if cblock.y_flag else 0

    try:
        self.execute("G21",lineno())
        self.execute("G64 P0.001",lineno())
        self.execute("G3456",lineno()) # raise error

        if self.z_flag:
            self.execute("G0 X%g Y%g" % (x0,y0))
            feed = self.feed_rate
            self.execute("F%g G1 Z%g" % (feed * plunge, self.z_number))
            self.execute("F%g" % (feed)) # restore feed


    except InterpreterException,e:
        print "InterpreterException"
        print 'Error message:', e.error_message
        print 'Line number:',e.line_number
        print 'Line text:',e.line_text
        print_exc_plus()
        return INTERP_ERROR


    return INTERP_OK

    ## n = self.execute("G3450 x%g Y%g Z%g" % (self.params["_[config]xpos"],
    ##                                  self.params["_[config]ypos"],
    ##                                  self.params["_[config]zpos"]))
    ## if n != INTERP_OK:
    ##     error_stack(self)
    ##     self.set_errormsg("interpreter operation failed: '%s'" % (self.get_errormsg()))
    ##     return INTERP_ERROR

    ## self.execute("(debug,still alive)")
    ## return INTERP_OK


    ## plunge_slowdown = 0.3
    ## z = words['z']  # plunge level
    ## p = words['p']  if words.has_key('q') else 0.1   # Z safety height
    ## a = words['q'] if words.has_key('q') else 0.1

    ## line = self.blocks[self.remap_level].line_number
    ## STRAIGHT_TRAVERSE(line,x0,y0,z + p,0,0,0,0,0,0) # rapid at safety height
    ## enqueue_SET_FEED_RATE(self.feed_rate * plunge_slowdown)
    ## STRAIGHT_FEED(line,x0,y0,z,0,0,0,0,0,0) # plunge
    ## enqueue_SET_FEED_RATE(self.feed_rate)

    ## return INTERP_OK
    ## for i in range(100):
    ##     t = i/10.
    ##     x = x0 + a * (cos(t) + t * sin(t))
    ##     y = y0 + a * (sin(t) - t * cos(t))
    ##     STRAIGHT_FEED(line,x,y,z,0,0,0,0,0,0)



## {{{ http://code.activestate.com/recipes/52215/ (r1)

def print_exc_plus():
    """
    Print the usual traceback information, followed by a listing of all the
    local variables in each frame.
    """
    tb = sys.exc_info()[2]
    while 1:
        if not tb.tb_next:
            break
        tb = tb.tb_next
    stack = []
    f = tb.tb_frame
    while f:
        stack.append(f)
        f = f.f_back
    stack.reverse()
    traceback.print_exc()
    print "Locals by frame, innermost last"
    for frame in stack:
        print
        print "Frame %s in %s at line %s" % (frame.f_code.co_name,
                                             frame.f_code.co_filename,
                                             frame.f_lineno)
        for key, value in frame.f_locals.items():
            print "\t%20s = " % key,
            #We have to be careful not to cause a new error in our error
            #printer! Calling str() on an unknown object could cause an
            #error we don't want.
            try:
                print value
            except:
                print "<ERROR WHILE PRINTING VALUE>"
