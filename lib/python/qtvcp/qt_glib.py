#!/usr/bin/env python
# vim: sts=4 sw=4 et

import _hal, hal
from PyQt5.QtCore import QObject, QTimer, pyqtSignal
from hal_glib import _GStat as GladeVcpStat
import linuxcnc
import math
import gobject

# using pyqt5 reqires using super()
# because QObject uses a differnt calling signature it must be _init_ last with out using Super
# reference: https://fuhm.net/super-harmful/
class QPin(hal.Pin,QObject ):
    value_changed = pyqtSignal([int], [float], [bool] )

    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        super(QPin,self).__init__(*a, **kw)
        QObject.__init__(self,None)
        self._item_wrap(self._item)
        self._prev = None
        self.REGISTRY.append(self)
        self.update_start()

    def update(self):
        tmp = self.get()
        if tmp != self._prev:
            self.value_changed.emit(tmp)
        self._prev = tmp

    @classmethod
    def update_all(self):
        if not self.UPDATE:
            return
        kill = []
        for p in self.REGISTRY:
            try:
                p.update()
            except Exception, e:
                kill.append(p)
                print "Error updating pin %s; Removing" % p
                print e
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

class QComponent:
    def __init__(self, comp):
        if isinstance(comp, QComponent):
            comp = comp.comp
        self.comp = comp

    def newpin(self, *a, **kw): return QPin(_hal.component.newpin(self.comp, *a, **kw))
    def getpin(self, *a, **kw): return QPin(_hal.component.getpin(self.comp, *a, **kw))

    def exit(self, *a, **kw): return self.comp.exit(*a, **kw)

    def __getitem__(self, k): return self.comp[k]
    def __setitem__(self, k, v): self.comp[k] = v

# use the same Gstat as gladeVCP uses
# by subclassing it
class _GStat(GladeVcpStat):
    def __init__(self):
        super(_GStat, self).__init__()

    def add_timer(self):
        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(100)

# used so all qtvcp widgets use the same instance of _gstat
# this keeps them all in synch
# if you load more then one instance of QTvcp/Qtscreen each one has
# it's own instance of _gstat
class GStat(_GStat):
    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _GStat.__new__(cls, *args, **kwargs)
        return cls._instance

