import emccanon
import interpreter

def failingprolog(self,*args, **words):
    emccanon.CANON_ERROR("%s is a literal percent s")
    self.set_errormsg("A failed Python prolog returning INTERP_ERROR")
    return interpreter.INTERP_ERROR


