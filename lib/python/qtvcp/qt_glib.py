#!/usr/bin/env python
# vim: sts=4 sw=4 et

import _hal, hal
from PyQt4.QtCore import QObject, QTimer, pyqtSignal
import linuxcnc

class GPin(QObject, hal.Pin):
    value_changed = pyqtSignal()

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
            self.emit('value-changed')
        self._prev = tmp

    @classmethod
    def update_all(self):
        if not self.UPDATE:
            return
        kill = []
        for p in self.REGISTRY:
            try:
                p.update()
            except:
                kill.append(p)
                print "Error updating pin %s; Removing" % p
        for p in kill:
            self.REGISTRY.remove(p)
        return self.UPDATE

    @classmethod
    def update_start(self, timeout=100):
        if GPin.UPDATE:
            return
        GPin.UPDATE = True
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_all)
        self.timer.start(100)

    @classmethod
    def update_stop(self, timeout=100):
        GPin.UPDATE = False

class GComponent:
    def __init__(self, comp):
        if isinstance(comp, GComponent):
            comp = comp.comp
        self.comp = comp

    def newpin(self, *a, **kw): return GPin(_hal.component.newpin(self.comp, *a, **kw))
    def getpin(self, *a, **kw): return GPin(_hal.component.getpin(self.comp, *a, **kw))

    def exit(self, *a, **kw): return self.comp.exit(*a, **kw)

    def __getitem__(self, k): return self.comp[k]
    def __setitem__(self, k, v): self.comp[k] = v

class _GStat(QObject):
    '''Emits signals based on linuxcnc status '''
    state_estop = pyqtSignal()
    state_estop_reset = pyqtSignal()
    state_on = pyqtSignal()
    state_off = pyqtSignal()
    homed = pyqtSignal(str)
    all_homed = pyqtSignal()
    not_all_homed = pyqtSignal(str)

    mode_manual = pyqtSignal()
    mode_auto = pyqtSignal()
    mode_mdi = pyqtSignal()

    interp_run = pyqtSignal()
    interp_idle = pyqtSignal()
    interp_paused = pyqtSignal()
    interp_reading = pyqtSignal()
    interp_waiting = pyqtSignal()

    file_loaded = pyqtSignal(str)
    reload_display = pyqtSignal()
    line_changed = pyqtSignal(int)
    tool_in_spindle_changed = pyqtSignal(int)

    STATES = { linuxcnc.STATE_ESTOP:       'state_estop'
             , linuxcnc.STATE_ESTOP_RESET: 'state_estop_reset'
             , linuxcnc.STATE_ON:          'state_on'
             , linuxcnc.STATE_OFF:         'state_off'
             }

    MODES  = { linuxcnc.MODE_MANUAL: 'mode_manual'
             , linuxcnc.MODE_AUTO:   'mode_auto'
             , linuxcnc.MODE_MDI:    'mode_mdi'
             }

    INTERP = { linuxcnc.INTERP_WAITING: 'interp_waiting'
             , linuxcnc.INTERP_READING: 'interp_reading'
             , linuxcnc.INTERP_PAUSED:  'interp_paused'
             , linuxcnc.INTERP_IDLE:    'interp_idle'
             }

    def __init__(self, stat = None):
        QObject.__init__(self)
        self.stat = stat or linuxcnc.stat()
        self.old = {}
        try:
            self.stat.poll()
            self.merge()
        except:
            pass
        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(100)

    def merge(self):
        self.old['state'] = self.stat.task_state
        self.old['mode']  = self.stat.task_mode
        self.old['interp']= self.stat.interp_state
        # Only update file if call_level is 0, which
        # means we are not executing a subroutine/remap
        # This avoids emitting signals for bogus file names below 
        if self.stat.call_level == 0:
            self.old['file']  = self.stat.file
        self.old['line']  = self.stat.motion_line
        self.old['homed'] = self.stat.homed
        self.old['tool_in_spindle'] = self.stat.tool_in_spindle

    def update(self):
        try:
            self.stat.poll()
        except:
            # Reschedule
            return True
        old = dict(self.old)
        self.merge()

        state_old = old.get('state', 0)
        state_new = self.old['state']
        if not state_old:
            if state_new > linuxcnc.STATE_ESTOP:
                self.state_estop_reset.emit()
            else:
                self.state_estop.emit()
            self.state_off.emit()
            self.interp_idle.emit()

        if state_new != state_old:
            if state_old == linuxcnc.STATE_ON and state_new < linuxcnc.STATE_ON:
                self.state_off.emit()
            self[self.STATES[state_new]].emit()
            if state_new == linuxcnc.STATE_ON:
                old['mode'] = 0
                old['interp'] = 0

        mode_old = old.get('mode', 0)
        mode_new = self.old['mode']
        if mode_new != mode_old:
            self[self.MODES[mode_new]].emit()

        interp_old = old.get('interp', 0)
        interp_new = self.old['interp']
        if interp_new != interp_old:
            if not interp_old or interp_old == linuxcnc.INTERP_IDLE:
                print "Emit", "interp_run"
                self.interp_run.emit()
            self[self.INTERP[interp_new]].emit()

        file_old = old.get('file', None)
        file_new = self.old['file']
        if file_new != file_old:
            # if interpreter is reading or waiting, the new file
            # is a remap procedure, with the following test we
            # partly avoid emitting a signal in that case, which would cause 
            # a reload of the preview and sourceview widgets.  A signal could
            # still be emitted if aborting a program shortly after it ran an
            # external file subroutine, but that is fixed by not updating the
            # file name if call_level != 0 in the merge() function above.
            if self.stat.interp_state == linuxcnc.INTERP_IDLE:
                self.file_loaded.emit(file_new)

        #ToDo : Find a way to avoid signal when the line changed due to 
        #       a remap procedure, because the signal do highlight a wrong
        #       line in the code
        # I think this might be fixed somewhere, because I do not see bogus line changed signals
        # when running an external file subroutine.  I tried making it not record line numbers when
        # the call level is non-zero above, but then I was not getting nearly all the signals I should
        # Moses McKnight
        line_old = old.get('line', None)
        line_new = self.old['line']
        if line_new != line_old:
            self.line_changed.emit(line_new)

        tool_old = old.get('tool_in_spindle', None)
        tool_new = self.old['tool_in_spindle']
        if tool_new != tool_old:
            self.tool_in_spindle_changed.emit(tool_new)

        # if the homed status has changed
        # check number of homed axes against number of available axes
        # if they are equal send the all_homed signal
        # else not_all_homed (with a string of unhomed joint numbers)
        # if a joint is homed send 'homed' (with a string of homed joint numbers)
        homed_old = old.get('homed', None)
        homed_new = self.old['homed']
        if homed_new != homed_old:
            axis_count = count = 0
            unhomed = homed = ""
            for i,h in enumerate(homed_new):
                if h:
                    count +=1
                    homed += str(i)
                if self.stat.axis_mask & (1<<i) == 0: continue
                axis_count += 1
                if not h:
                    unhomed += str(i)
            if count:
                self.homed.emit(homed)
            if count == axis_count:
                self.all_homed.emit()
            else:
                self.not_all_homed.emit(unhomed)

        return True

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class GStat(_GStat):
    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _GStat.__new__(cls, *args, **kwargs)
        return cls._instance
