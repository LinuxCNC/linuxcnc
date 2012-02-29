import oword
import interpreter

def __init__(self):
    if hasattr(interpreter,'this'):
        if self != interpreter.this:
            print "__init__: self != this"
    else:
        print "__init__: no 'this' attribute"
        
