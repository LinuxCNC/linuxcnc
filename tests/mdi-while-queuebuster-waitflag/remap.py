#!/usr/bin/env python3
import linuxcnc
import emccanon
import interpreter

def init_stdglue(self):
    pass

def M400(self,**words):

    self.execute("M66 E0 L0")
    emccanon.MESSAGE("M400 before the yield")
    yield interpreter.INTERP_EXECUTE_FINISH

    emccanon.MESSAGE("M400 after the yield")


