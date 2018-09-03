#

from .hal cimport *
from .hal_rcomp cimport *
from os import strerror


class CompiledComponent(Component):
    def __cinit__(self):
        pass

class RemoteComponent(Component):
    def __new__(cls, name, timer=100, acceptdefaults=False, noexit=True, **kwargs):
        compargs = 0
        if acceptdefaults:
            compargs |= RCOMP_ACCEPT_VALUES_ON_BIND
        inst = Component.__new__(cls, name, mode=TYPE_REMOTE, noexit=noexit,
                                 userarg1=timer, userarg2=compargs, **kwargs)
        return inst

    def __init__(self, name, timer=100, acceptdefaults=False, noexit=True):
        pass
