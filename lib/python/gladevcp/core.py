#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import linuxcnc
from gi.repository import GObject

import inspect
import _hal
import hal
import traceback

from common.hal_glib import GStat
from common.iniinfo import _IStat as IStatParent

# Set up logging
from common import logger
log = logger.getLogger(__name__)
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL, VERBOSE

################################################################
# IStat class
################################################################
class Info(IStatParent):
    _instance = None
    _instanceNum = 0

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = IStatParent.__new__(cls, *args, **kwargs)
        return cls._instance

################################################################
# GStat class
################################################################
# use the same Gstat as gladeVCP uses
# by subclassing it
class Status(GStat):
    _instance = None
    _instanceNum = 0
    __gsignals__ = {
        'toolfile-stale': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
    }
    TEMPARARY_MESSAGE = 255 # remove in the future
    OPERATOR_ERROR = linuxcnc.OPERATOR_ERROR
    OPERATOR_TEXT = linuxcnc.OPERATOR_TEXT
    NML_ERROR = linuxcnc.NML_ERROR
    NML_TEXT = linuxcnc.NML_TEXT

    # only make one instance of the class - pass it to all other
    # requested instances
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = GStat.__new__(cls, *args, **kwargs)
        return cls._instance

    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
        self.__class__._instanceNum += 1
        super(Status, self).__init__()

################################################################
# Lcnc_Action class
################################################################
from gladevcp.gtk_action import _Lcnc_Action as _ActionParent


class Action(_ActionParent):
    _instance = None
    _instanceNum = 0
    
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _ActionParent.__new__(cls, *args, **kwargs)
        return cls._instance

