#

from .hal cimport *
from .hal_rcomp cimport *
from os import strerror


class CompiledComponent(Component):
    def __cinit__(self):
        pass
