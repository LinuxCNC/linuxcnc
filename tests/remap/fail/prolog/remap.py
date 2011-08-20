import emccanon
import interpreter

def failingprolog(self,*args, **words):
    emccanon.MESSAGE("failingprolog returning INTERP_ERROR")
    self.set_errormsg("A failed Python prolog returning INTERP_ERROR")
    return interpreter.INTERP_ERROR


