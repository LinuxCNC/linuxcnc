#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import linuxcnc
from gi.repository import GObject

import inspect
import _hal
import hal
import traceback

from hal_glib import GStat
from qtvcp.qt_istat import _IStat as IStatParent

# Set up logging
from qtvcp import logger
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


# Now that the class is defined create a reference to it for the other classes
INI = Info()

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
    TEMPARARY_MESSAGE = 255
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
        GObject.Object.__init__(self)
        self.__class__._instanceNum += 1
        super(GStat, self).__init__()
        self.current_jog_rate = INI.DEFAULT_LINEAR_JOG_VEL
        self.angular_jog_velocity = INI.DEFAULT_ANGULAR_JOG_VEL

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

