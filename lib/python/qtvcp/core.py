#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import linuxcnc
from gi.repository import GObject

import inspect
import _hal
import hal
import traceback
from PyQt5.QtCore import QObject, QTimer, pyqtSignal

from common.hal_glib import GStat
from common.iniinfo import _IStat as IStatParent

from qtvcp.qt_halobjects import Qhal as QHAL
from qtvcp.qt_halobjects import QPin as QPIN

# Set up logging
from common import logger
log = logger.getLogger(__name__)

# Force the log level for this module
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

    # get filter extensions in QT format
    def get_qt_filter_extensions(self):
        all_extensions = []
        try:
            for k, v in self.PROGRAM_FILTERS:
                k = k.replace('.', ' *.')
                k = k.replace(',', ' ')
                all_extensions.append((';;%s (%s)' % (v, k)))
            all_extensions.append((';;All (*)'))
            temp = ''
            for i in all_extensions:
                temp = '%s %s' % (temp, i)
            return temp
        except Exception as e:
            log.warning('Qt filter Extension Parsing Error: {}\n Using Default: ALL (*)'.format(e))
            return ('All (*)')

###############################################################
# HAL Object clases
###############################################################
class QPin(QPIN):
    def __init__(self, *a, **kw):
        super(QPin, self).__init__(*a, **kw)

class Qhal(QHAL):
    def __init__(self, comp=None, hal=None):
        super(Qhal, self).__init__(comp, hal)
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

        # can only have ONE error channel instance in qtvcp
        self.ERROR = linuxcnc.error_channel()
        self._block_polling = False

    # we override this function from hal_glib
    # add replace it with setTimer()
    def set_timer(self, cycleTime=None):
        pass

    def setTimer(self, cycleTime=None):
        GObject.threads_init()
        GObject.timeout_add(int(cycleTime), self.update)

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



