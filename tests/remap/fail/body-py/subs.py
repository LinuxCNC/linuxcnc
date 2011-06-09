from InterpMod import *

def interp_error(userdata, **words):
    interp.set_errormsg("A failed Python remap handler returning INTERP_ERROR")
    return INTERP_ERROR


