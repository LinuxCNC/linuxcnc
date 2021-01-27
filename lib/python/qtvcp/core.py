#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import linuxcnc
import inspect

import sys
if sys.version_info.major > 2:
    from gi.repository import GObject
    from gi.repository import GLib
    gobj_run_first = GObject.SignalFlags.RUN_FIRST
else:
    import gobject as GObject
    gobj_run_first = GObject.SIGNAL_RUN_FIRST

import _hal
import hal
from PyQt5.QtCore import QObject, QTimer, pyqtSignal
from hal_glib import GStat
from qtvcp.qt_istat import _IStat as IStatParent

# Set up logging
from . import logger
log = logger.getLogger(__name__)
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL, VERBOSE


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
    def update_all(self):
        if not self.UPDATE:
            return
        kill = []
        for p in self.REGISTRY:
            try:
                p.update()
            except Exception as e:
                kill.append(p)
                log.error("Error updating pin {}; Removing".format(p))
                log.exception(e)
        for p in kill:
            self.REGISTRY.remove(p)
        return self.UPDATE

    @classmethod
    def update_start(self, timeout=100):
        if QPin.UPDATE:
            return
        QPin.UPDATE = True
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_all)
        self.timer.start(100)

    @classmethod
    def update_stop(self, timeout=100):
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


class QComponent:
    def __init__(self, comp):
        if isinstance(comp, QComponent):
            comp = comp.comp
        self.comp = comp

    def newpin(self, *a, **kw):
        try:
            p = QPin(_hal.component.newpin(self.comp, *a, **kw))
        except Exception as e:
            if log.getEffectiveLevel() == logger.VERBOSE:
                raise
            t = inspect.getframeinfo(inspect.currentframe().f_back)
            log.error("QComponent: Error making new HAL pin: {}\n    {}\n    Line {}\n    Function: {}".
                format(e, t[0], t[1], t[2]))
            p = DummyPin(*a, ERROR=e)
        return p

    def getpin(self, *a, **kw): return QPin(_hal.component.getpin(self.comp, *a, **kw))

    def exit(self, *a, **kw): return self.comp.exit(*a, **kw)

    def __getitem__(self, k): return self.comp[k]
    def __setitem__(self, k, v): self.comp[k] = v


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
        'toolfile-stale': (gobj_run_first, GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
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
        self.current_jog_rate = INI.DEFAULT_LINEAR_JOG_VEL
        self.angular_jog_velocity = INI.DEFAULT_ANGULAR_JOG_VEL

    # we override this function from hal_glib
    def set_timer(self):
        if sys.version_info.major > 2:
            GLib.timeout_add(100, self.update)
            return
        # TODO Python 2 needs the threads_init. Python3 this is deprecated and not needed
        # View new functionality here: https://pygobject.readthedocs.io/en/latest/guide/threading.html
        # If we want to keep the time out it is now GLib.timeout_add(100, self.update)
        GObject.threads_init()
        GObject.timeout_add(100, self.update)


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
