from InterpMod import *

def failingprolog(userdata, **words):
    interp.set_errormsg("A failed Python prolog returning INTERP_ERROR")
    return INTERP_ERROR


