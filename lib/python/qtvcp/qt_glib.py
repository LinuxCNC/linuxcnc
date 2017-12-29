#!/usr/bin/env python
# vim: sts=4 sw=4 et

import linuxcnc
import math
import gobject

import _hal, hal
from PyQt5.QtCore import QObject, QTimer, pyqtSignal
from hal_glib import _GStat as GladeVcpStat
from qtvcp.qt_istat import IStat
INI = IStat()

# Set up logging
import logger
log = logger.getLogger(__name__)
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class QPin(hal.Pin, QObject):
    value_changed = pyqtSignal([int], [float], [bool] )

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
        self.current_jog_rate = INI.DEFAULT_LINEAR_JOG_VEL

    # we override this function from hal_glib
    #TODO why do we need to do this with qt5 and not qt4?
    # seg fault without it
    def set_timer(self):
        gobject.threads_init()
        gobject.timeout_add(100, self.update)

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

################################################################
# Action class
################################################################
class Lcnc_Action():
    def __init__(self):
        self.cmd = linuxcnc.command()
        self.gstat = GStat()
        self.tmp = None

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
        log.info('Homing Joint: {}'.format(joint))
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
        flt = INI.get_filter_program(str(fname))
        if not flt:
            self.cmd.program_open(str(fname))
        else:
            self.open_filter_program(str(fname), flt)
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
            log.debug('resume')
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

    def RECORD_CURRENT_MODE(self):
        mode = self.gstat.get_current_mode()
        self.last_mode = mode
        return mode

    def RESTORE_RECORDED_MODE(self):
        self.ensure_mode(self.last_mode)

    ######################################
    # Action Helper functions
    ######################################

    def ensure_mode(self, *modes):
        truth, premode = self.gstat.check_for_modes(modes)
        if truth is False:
            self.cmd.mode(modes[0])
            self.cmd.wait_complete()
            return (True, premode)
        else:
            return (truth, premode)

    def open_filter_program(self,fname, flt):
        if not self.tmp:
            self._mktemp()
        tmp = os.path.join(self.tmp, os.path.basename(fname))
        print 'temp',tmp
        flt = FilterProgram(flt, fname, tmp, lambda r: r or self._load_filter_result(tmp))

    def _load_filter_result(self, fname):
        if fname:
            self.cmd.program_open(fname)

    def _mktemp(self):
        if self.tmp:
            return
        self.tmp = tempfile.mkdtemp(prefix='emcflt-', suffix='.d')
        atexit.register(lambda: shutil.rmtree(self.tmp))

########################################################################
# Filter Class
########################################################################
import os, sys, time, select, re
import tempfile, atexit, shutil

# slightly reworked code from gladevcp
# loads a filter program and collects the result
progress_re = re.compile("^FILTER_PROGRESS=(\\d*)$")
class FilterProgram:
    def __init__(self, program_filter, infilename, outfilename, callback=None):
        import subprocess
        outfile = open(outfilename, "w")
        infilename_q = infilename.replace("'", "'\\''")
        env = dict(os.environ)
        env['AXIS_PROGRESS_BAR'] = '1'
        p = subprocess.Popen(["sh", "-c", "%s '%s'" % (program_filter, infilename_q)],
                              stdin=subprocess.PIPE,
                              stdout=outfile,
                              stderr=subprocess.PIPE,
                              env=env)
        p.stdin.close()  # No input for you
        self.gstat = GStat()
        self.p = p
        self.stderr_text = []
        self.program_filter = program_filter
        self.callback = callback
        self.gid = self.gstat.connect('periodic', self.update)
        #progress = Progress(1, 100)
        #progress.set_text(_("Filtering..."))

    def update(self, w):
        if self.p.poll() is not None:
            self.finish()
            self.gstat.disconnect(self.gid)
            return False

        r,w,x = select.select([self.p.stderr], [], [], 0)
        if not r:
            return True
        stderr_line = self.p.stderr.readline()
        m = progress_re.match(stderr_line)
        if m:
            pass #progress.update(int(m.group(1)), 1)
        else:
            self.stderr_text.append(stderr_line)
            sys.stderr.write(stderr_line)
        return True

    def finish(self):
        # .. might be something left on stderr
        for line in self.p.stderr:
            m = progress_re.match(line)
            if not m:
                self.stderr_text.append(line)
                sys.stderr.write(line)
        r = self.p.returncode
        if r:
            self.error(r, "".join(self.stderr_text))
        if self.callback:
            self.callback(r)

    def error(self, exitcode, stderr):
        dialog = gtk.MessageDialog(None, 0, gtk.MESSAGE_ERROR, gtk.BUTTONS_CLOSE,
                _("The program %(program)r exited with code %(code)d.  "
                "Any error messages it produced are shown below:")
                    % {'program': self.program_filter, 'code': exitcode})
        dialog.format_secondary_text(stderr)
        dialog.run()
        dialog.destroy()

