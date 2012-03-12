import oword
import interpreter

def __init__(self):
    if hasattr(interpreter,'this'):
        if self is not interpreter.this:
            print "__init__: self not is this"
    else:
        print "__init__: no 'this' attribute"
        
