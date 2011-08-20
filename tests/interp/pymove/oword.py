import emccanon
import interpreter
import inspect


## {{{ http://code.activestate.com/recipes/145297/ (r1)
def lineno():
    """Returns the current line number in our program."""
    return inspect.currentframe().f_back.f_lineno

# example for creating moves in a Python oword sub
def canonmove(self, x,y,z):
    emccanon.STRAIGHT_FEED(lineno(),x,y,z,0,0,0,0,0,0)
    emccanon.STRAIGHT_TRAVERSE(lineno(),self.params["_[config]xpos"],self.params["_[config]ypos"],self.params["_[config]zpos"],0,0,0,0,0,0)
    return 0.0

