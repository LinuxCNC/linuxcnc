from InterpMod import *

def failingepilog(userdata, **words):
    CanonMod.MESSAGE("failing_epilog returning INTERP_ERROR")
    interp.set_errormsg("A failed Python epilog returning INTERP_ERROR")
    return INTERP_ERROR


