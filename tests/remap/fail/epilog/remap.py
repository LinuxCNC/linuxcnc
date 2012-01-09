import emccanon
import interpreter

def failingepilog(self,*args, **words):
    emccanon.MESSAGE("failing_epilog returning INTERP_ERROR")
    self.set_errormsg("A failed Python epilog returning INTERP_ERROR")
    return interpreter.INTERP_ERROR


