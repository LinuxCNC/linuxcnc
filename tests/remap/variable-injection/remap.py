from interpreter import *


def prolog405(self, **words):
    self.params["foo405"] = 42
    return INTERP_OK

# rm405 will bump foo405 by 1

def epilog405(self, **words):
    if self.params["foo405"] != 43:
        self.set_errormsg("foo405 != 43 in epilog405")
        return INTERP_ERROR
    return INTERP_OK


def prolog406(self, **words):
    self.params["foo406"] = 42
    return INTERP_OK

# rm406 will bump foo406 by 1

def epilog406(self, **words):
    if self.params["foo406"] != 43:
        self.set_errormsg("foo406 != 43 in epilog406")
        return INTERP_ERROR
    return INTERP_OK


def prolog407(self, **words):
    self.params["foo407"] = 42
    return INTERP_OK

# rm407 will bump foo407 by 1

def epilog407(self, **words):
    if self.params["foo407"] != 43:
        self.set_errormsg("foo407 != 43 in epilog407")
        return INTERP_ERROR
    return INTERP_OK

