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

# The order of these classes is importanr, otherwise - circular imports.
# the some of the later classes reference the earlier classes

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
    pinValueChanged = pyqtSignal('PyQt_PyObject','PyQt_PyObject')
    isDrivenChanged = pyqtSignal('PyQt_PyObject','PyQt_PyObject')
    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        super(QPin, self).__init__(*a, **kw)
        QObject.__init__(self, None)
        self.hal = hal
        self._item_wrap(self._item)
        self._prev = None
        self._prevDriven = None
        self.REGISTRY.append(self)
        self.update_start()
        self.prefix = None

    def update(self):
        tmp = self.get()
        if tmp != self._prev:
            self.value_changed.emit(tmp)
            self.pinValueChanged.emit(self, tmp)
        self._prev = tmp

    def isDriven(self):
        return hal.pin_has_writer('{}.{}'.format(self.prefix, self.get_name()))

    def updateDriven(self):
        tmp = self.isDriven()
        if tmp != self._prevDriven:
            self.isDrivenChanged.emit(self, tmp)
        self._prevDriven = tmp

    def text(self):
        return self.get_name()

    # always returns False because
    # there was no error when making pin
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
            if p.receivers(p.isDrivenChanged) > 0:
                p.updateDriven()

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
    # there was an error when making HAL pin
    # see class QPin
    def error(self):
        return True

    def getError(self):
        print('{}'.format(self._kw.get('ERROR')))

    def get(self):
        return 0

    def set(self, *a, **kw):
        pass

    def get_name(self):
        return self._a[0]

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
        except ValueError as e:
            # if pin is already made, find a new name
            if 'Duplicate pin name' in '{}'.format(e):
                try:
                    # tuples are immutable, convert to list
                    y = list(a)
                    y[0] = self.makeUniqueName(y[0])
                    a = tuple(y)
                    # this late in the game, component is probably already 'ready'
                    if self.hal.component_is_ready(self.comp.getprefix()):
                        self.comp.unready()
                        p = QPin(_hal.component.newpin(self.comp, *a, **kw))
                        self.comp.ready()
                    else:
                        p = QPin(_hal.component.newpin(self.comp, *a, **kw))
                except Exception as e:
                    raise
        except Exception as e:
            if log.getEffectiveLevel() == logger.VERBOSE:
                raise
            t = inspect.getframeinfo(inspect.currentframe().f_back)
            log.error("Qhal: Error making new HAL pin: {}\n    {}\n    Line {}\n    Function: {}".
                format(e, t[0], t[1], t[2]))
            log.error("Qhal: {}".format(traceback.format_exc()))
            p = DummyPin(*a, ERROR=e)
        p.prefix = self.comp.getprefix()
        return p

    def getpin(self, *a, **kw): return QPin(_hal.component.getpin(self.comp, *a, **kw))

    def getvalue(self, name):
        try:
            return hal.get_value(name)
        except Exception as e:
            raise("Qhal: Error getting value of {}\n {}".format(name, e))

    def setp(self,name, value):
        try:
            return hal.set_p(name,value)
        except Exception as e:
            raise("Qhal: Error setting pin {} to {}\n {}".format(name,value, e))

    def sets(self,name, value):
        try:
            return hal.set_s(name,value)
        except Exception as e:
            raise("Qhal: Error setting signal {} to {}\n {}".format(name,value, e))

    def exit(self, *a, **kw): return self.comp.exit(*a, **kw)

    # find a unique HAL pin name by adding '-x' to the base name
    # x being an ever increasing number till name is unique
    def makeUniqueName(self, name):
        num = 2
        base = self.comp.getprefix()
        while True:
            trial = ('{}.{}-{}').format(base,name,num)
            for i in hal.get_info_pins():
                if i['NAME'] == trial:
                    num +=1
                    trial = ('{}.{}-{}').format(base,name,num)
                    break
            else:
                break
        return ('{}-{}').format(name,num)

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

        # set the default jog speeds before the forced update
        self.current_jog_rate = INI.DEFAULT_LINEAR_JOG_VEL
        self.current_angular_jog_rate = INI.DEFAULT_ANGULAR_JOG_VEL

        # can only have ONE error channel instance in qtvcp
        self.ERROR = linuxcnc.error_channel()
        self._block_polling = False

    # we override this function from hal_glib
    def set_timer(self):
        GObject.threads_init()
        GObject.timeout_add(int(INI.CYCLE_TIME), self.update)

    # error polling is usually set up by screen_option widget
    # to call this function
    # but when using MDI subprograms, the subprogram must be the only
    # polling instance.
    # this is done by blocking the main screen polling until the
    # subprogram is done.
    def poll_error(self):
        if self._block_polling: return None
        return self.ERROR.poll()

    def block_error_polling(self, name=''):
        if name: print(name,'block')
        self._block_polling = True

    def unblock_error_polling(self,name=''):
        if name: print(name,'unblock')
        self._block_polling = False

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



