# trivial example
def _pi(self, *args):
    return 3.1415926535

# the Python equivalent of '#<_motion_mode> :
# return the currently active motion code (times 10)
def _py_motion_mode(self, *args):
    return self.active_g_codes[1]

def _error(self, *args):
    # this sets the error message and aborts execution (except in (debug,#<_err>))
    return "badly botched"

# return a whacky type to exercise the error reporting mechanism
def _badreturntype(self, *args):
    return object()
