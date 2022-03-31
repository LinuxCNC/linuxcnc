#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import linuxcnc
from gi.repository import GObject

import inspect
import _hal
import hal
import traceback
from PyQt5.QtCore import QObject, QTimer, pyqtSignal
from hal_glib import GStat
from qtvcp.qt_istat import _IStat as IStatParent

# Set up logging
from . import logger
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

class QPin(hal.Pin, QObject):

    value_changed = pyqtSignal('PyQt_PyObject')
    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        super(QPin, self).__init__(*a, **kw)
        QObject.__init__(self, None)
        self._item_wrap(self._item)
        self._prev = None
        self.REGISTRY.append(self)
        self.update_start()

    def update(self):
        tmp = self.get()
        if tmp != self._prev:
            self.value_changed.emit(tmp)
        self._prev = tmp

    def text(self):
        return self.get_name()

    # always returns False because
    # there was no errpr when making pin
    # see class DUMMY
    def error(self):
        return False

    @classmethod
    def update_all(cls):
        if not cls.UPDATE:
            return
        kill = []
        for p in cls.REGISTRY:
            try:
                p.update()
            except Exception as e:
                kill.append(p)
                log.error("Error updating pin {}; Removing".format(p))
                log.exception(e)
        for p in kill:
            cls.REGISTRY.remove(p)
        return cls.UPDATE

    @classmethod
    def update_start(cls):
        if QPin.UPDATE:
            return
        QPin.UPDATE = True
        cls.timer = QTimer()
        cls.timer.timeout.connect(cls.update_all)
        cls.timer.start(INI.HALPIN_CYCLE_TIME)

    @classmethod
    def update_stop(cls):
        QPin.UPDATE = False


# so errors when making QPins aren't fatal
class DummyPin(QObject):
    value_changed = pyqtSignal('PyQt_PyObject')

    def __init__(self, *a, **kw):
        super(DummyPin, self).__init__(None)
        self._a = a
        self._kw = kw

    # always returns True because
    # there was an errpr when making HAL pin
    # see class QPin
    def error(self):
        return True

    def getError(self):
        print('{}'.format(self._kw.get('ERROR')))

    def get(self):
        return 0

    def set(self, *a, **kw):
        pass


class _QHal(object):
    HAL_BIT = hal.HAL_BIT
    HAL_FLOAT = hal.HAL_FLOAT
    HAL_S32 = hal.HAL_S32
    HAL_U32 = hal.HAL_U32

    HAL_IN = hal.HAL_IN
    HAL_OUT = hal.HAL_OUT
    HAL_IO = hal.HAL_IO
    HAL_RO = hal.HAL_RO
    HAL_RW = hal.HAL_RW

    def __new__(cls, *a, **kw):
        instance = super(_QHal, cls).__new__(cls)
        instance.__init__(*a, **kw)
        return instance

    def __init__(self, comp=None, hal=None):
        # only initialize once for all instances
        if not self.__class__._instance is None:
            return

        if isinstance(comp, _QHal):
            comp = comp.comp
        self.comp = comp
        self.hal = hal

    def newpin(self, *a, **kw):
        try:
            p = QPin(_hal.component.newpin(self.comp, *a, **kw))
        except Exception as e:
            if log.getEffectiveLevel() == logger.VERBOSE:
                raise
            t = inspect.getframeinfo(inspect.currentframe().f_back)
            log.error("Qhal: Error making new HAL pin: {}\n    {}\n    Line {}\n    Function: {}".
                format(e, t[0], t[1], t[2]))
            log.error("Qhal: {}".format(traceback.format_exc()))
            p = DummyPin(*a, ERROR=e)
        return p

    def getpin(self, *a, **kw): return QPin(_hal.component.getpin(self.comp, *a, **kw))

    def getvalue(self, name):
        try:
            return hal.get_value(name)
        except Exception as e:
            log.error("Qhal: Error getting value of {}\n {}".format(name, e))

    def setp(self,name, value):
        try:
            return hal.set_p(name,value)
        except Exception as e:
            log.error("Qhal: Error setting value of {} to {}\n {}".format(name,value, e))

    def exit(self, *a, **kw): return self.comp.exit(*a, **kw)

    def __getitem__(self, k): return self.comp[k]
    def __setitem__(self, k, v): self.comp[k] = v

class Qhal(_QHal):
    _instance = None

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = _QHal.__new__(cls, *args, **kwargs)
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

    # we override this function from hal_glib
    # TODO why do we need to do this with qt5 and not qt4?
    # seg fault without it
    def set_timer(self):
        GObject.threads_init()
        GObject.timeout_add(int(INI.CYCLE_TIME), self.update)


################################################################
# Lcnc_Action class
################################################################
from qtvcp.qt_action import _Lcnc_Action as _ActionParent


class Action(_ActionParent):
    _instance = None
    _instanceNum = 0
    
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _ActionParent.__new__(cls, *args, **kwargs)
        return cls._instance


################################################################
# TStat class
################################################################
from qtvcp.qt_tstat import _TStat as _TStatParent


class Tool(_TStatParent):
    _instance = None
    _instanceNum = 0

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _TStatParent.__new__(cls, *args, **kwargs)
        return cls._instance


################################################################
# PStat class
################################################################
from qtvcp.qt_pstat import _PStat as _PStatParent


class Path(_PStatParent):
    _instance = None
    _instanceNum = 0

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _PStatParent.__new__(cls, *args, **kwargs)
        return cls._instance
