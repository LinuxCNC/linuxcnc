import emccanon
import interpreter

def failingepilog(*args, **words):
    emccanon.MESSAGE("failing_epilog returning INTERP_ERROR")
    interpreter.this.set_errormsg("A failed Python epilog returning INTERP_ERROR")
    return interpreter.INTERP_ERROR
