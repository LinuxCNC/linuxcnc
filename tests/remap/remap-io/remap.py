#!/usr/bin/python2
import linuxcnc
import emccanon
import interpreter

def init_stdglue(self):
    pass

def io_output_M62(self,**words):

    self.execute("M62 P0")
    return interpreter.INTERP_OK

def io_output_M63(self,**words):

    self.execute("M63 P0")
    return interpreter.INTERP_OK

def io_output_M64(self,**words):

    self.execute("M64 P0")
    return interpreter.INTERP_OK

def io_output_M65(self,**words):

    self.execute("M65 P0")
    return interpreter.INTERP_OK

def io_input_M66(self,**words):

    self.execute("M66 E0 L0")
    yield interpreter.INTERP_EXECUTE_FINISH

    # optionally,
    # yield interpreter.INTERP_OK


