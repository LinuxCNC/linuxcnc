import emccanon
import interpreter
import inspect


## {{{ http://code.activestate.com/recipes/145297/ (r1)
def lineno():
    """Returns the current line number in our program."""
    return inspect.currentframe().f_back.f_lineno

# example for creating moves in a Python oword sub
def canonmove(x,y,z):
    i = interpreter.this
    emccanon.STRAIGHT_FEED(lineno(),x,y,z,0,0,0,0,0,0)
    emccanon.STRAIGHT_TRAVERSE(lineno(),i.params["_[config]xpos"],i.params["_[config]ypos"],i.params["_[config]zpos"],0,0,0,0,0,0)
    return 0.0
