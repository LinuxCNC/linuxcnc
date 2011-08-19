import interpreter

def interp_error(userdata, **words):
    interpreter.this.set_errormsg("A failed Python remap handler returning INTERP_ERROR")
    return interpreter.INTERP_ERROR
