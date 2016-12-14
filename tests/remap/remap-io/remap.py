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

    self.execute("M66 %(inp)s%(wait_mode)s" % dict(
            inp = ("P0" if words.has_key('p') else "E0"),
            wait_mode = (" L%d" % words['l'] if words.has_key('l') else ""),
            ))
    yield interpreter.INTERP_EXECUTE_FINISH

    # Result should be available after yield; save it for test to check
    self.params[100] = self.params[5399]
    yield interpreter.INTERP_OK


def io_output_M67(self,**words):

    self.execute("M67 E0 Q%.2f" % words['q'])
    return interpreter.INTERP_OK

def io_output_M68(self,**words):

    self.execute("M68 E0 Q%.2f" % words['q'])
    return interpreter.INTERP_OK

