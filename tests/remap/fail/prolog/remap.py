import emccanon
import interpreter

def failingprolog(*args, **words):
    emccanon.MESSAGE("failingprolog returning INTERP_ERROR")
    interpreter.this.set_errormsg("A failed Python prolog returning INTERP_ERROR")
    return interpreter.INTERP_ERROR
