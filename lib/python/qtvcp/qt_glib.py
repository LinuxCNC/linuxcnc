#!/usr/bin/env python
# vim: sts=4 sw=4 et

import linuxcnc
import math
import gobject

import _hal, hal
from PyQt4.QtCore import QObject, QTimer, pyqtSignal
from hal_glib import _GStat as GladeVcpStat
from qtvcp.qt_istat import IStat
INI = IStat()

class QPin(QObject, hal.Pin):
    value_changed = pyqtSignal([int], [float], [bool] )

    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        QObject.__init__(self)
        hal.Pin.__init__(self, *a, **kw)
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
        GladeVcpStat.__init__(self)
        self.current_jog_rate = INI.DEFAULT_LINEAR_JOG_VEL

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

class Lcnc_Action():
    def __init__(self):
        self.cmd = linuxcnc.command()
        self.gstat = GStat()

    def SET_ESTOP_STATE(self, state):
        if state:
            self.cmd.state(linuxcnc.STATE_ESTOP)
        else:
            self.cmd.state(linuxcnc.STATE_ESTOP_RESET)

    def SET_MACHINE_STATE(self, state):
        if state:
            self.cmd.state(linuxcnc.STATE_ON)
        else:
            self.cmd.state(linuxcnc.STATE_OFF)

    def SET_MACHINE_HOMING(self, joint):
        print 'Homing Joint:',joint
        self.ensure_mode(linuxcnc.MODE_MANUAL)
        self.cmd.teleop_enable(False)
        self.cmd.home(joint)

    def SET_MACHINE_UNHOMED(self, joint):
        self.ensure_mode(linuxcnc.MODE_MANUAL)
        self.cmd.teleop_enable(False)
        #self.cmd.traj_mode(linuxcnc.TRAJ_MODE_FREE)
        self.cmd.unhome(joint)

    def SET_AUTO_MODE(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)

    def SET_MDI_MODE(self):
        self.ensure_mode(linuxcnc.MODE_MDI)

    def SET_MANUAL_MODE(self):
        self.ensure_mode(linuxcnc.MODE_MANUAL)

    def CALL_MDI(self, code):
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi('%s'%code)

    def UPDATE_VAR_FILE(self):
        self.ensure_mode(linuxcnc.MODE_MANUAL)
        self.ensure_mode(linuxcnc.MODE_MDI)

    def OPEN_PROGRAM(self, fname):
        self.ensure_mode(linuxcnc.MODE_AUTO)
        self.cmd.program_open(str(fname))
        self.gstat.emit('reload-display')

    def SET_AXIS_ORIGIN(self,axis,value):
        m = "G10 L20 P0 %s%f"%(axis,value)
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        self.gstat.emit('reload-display')

    def RUN(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)
        self.cmd.auto(linuxcnc.AUTO_RUN,0)

    def ABORT(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)
        self.cmd.abort()

    def PAUSE(self):
        if not self.gstat.stat.paused:
            self.cmd.auto(linuxcnc.AUTO_PAUSE)
        else:
            print 'resume'
            self.cmd.auto(linuxcnc.AUTO_RESUME)

    def SET_RAPID_RATE(self, rate):
        self.cmd.rapidrate(rate/100.0)
    def SET_FEED_RATE(self, rate):
        self.cmd.feedrate(rate/100.0)
    def SET_SPINDLE_RATE(self, rate):
        self.cmd.spindleoverride(rate/100.0)
    def SET_JOG_RATE(self, rate):
        self.gstat.set_jog_rate(float(rate))
    def SET_JOG_INCR(self, incr):
        pass

    def ZERO_G92_OFFSET (self, widget):
        self.CALL_MDI("G92.1")
        self.gstat.emit('reload-display')
    def ZERO_ROTATIONAL_OFFSET(self, widget):
        self.CALL_MDI("G10 L2 P0 R 0")
        self.gstat.emit('reload-display')

    ###############################################################################
    # Helper functions
    ###############################################################################

    def RECORD_CURRENT_MODE(self):
        mode = self.gstat.get_current_mode()
        self.last_mode = mode
        return mode

    def RESTORE_RECORDED_MODE(self):
        self.ensure_mode(self.last_mode)

    def ensure_mode(self, *modes):
        truth, premode = self.gstat.check_for_modes(modes)
        if truth is False:
            self.cmd.mode(modes[0])
            self.cmd.wait_complete()
            return (True, premode)
        else:
            return (truth, premode)


