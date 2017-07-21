#!/usr/bin/env python
import linuxcnc
import emccanon
import interpreter

def init_stdglue(self):
    pass

def M400(self,**words):

    self.execute("M66 E0 L0")
    # self.execute("M66 P0 L1 Q1")
    print "self.input_flag pre = %s" % self.input_flag
    yield interpreter.INTERP_EXECUTE_FINISH

    print "self.input_flag post = %s" % self.input_flag
    # optionally,
    # yield interpreter.INTERP_OK


