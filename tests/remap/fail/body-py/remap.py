import interpreter

def interp_error(self,userdata, **words):
    self.set_errormsg("A failed Python remap handler returning INTERP_ERROR")
    return interpreter.INTERP_ERROR


