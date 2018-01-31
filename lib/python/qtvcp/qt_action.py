import linuxcnc

# Set up logging
import logger
log = logger.getLogger(__name__)
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

from qtvcp.core import Status, Info
INFO = Info()
STATUS = Status()

################################################################
# Action class
################################################################
class _Lcnc_Action(object):
    def __init__(self):
        self.cmd = linuxcnc.command()
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
        flt = INFO.get_filter_program(str(fname))
        if not flt:
            self.cmd.program_open(str(fname))
        else:
            self.open_filter_program(str(fname), flt)
        STATUS.emit('reload-display')

    def SET_AXIS_ORIGIN(self,axis,value):
        m = "G10 L20 P0 %s%f"%(axis,value)
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        STATUS.emit('reload-display')

    def RUN(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)
        self.cmd.auto(linuxcnc.AUTO_RUN,0)

    def ABORT(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)
        self.cmd.abort()

    def PAUSE(self):
        if not STATUS.stat.paused:
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
        STATUS.set_jog_rate(float(rate))
    def SET_JOG_INCR(self, incr):
        pass

    def ZERO_G92_OFFSET (self, widget):
        self.CALL_MDI("G92.1")
        STATUS.emit('reload-display')
    def ZERO_ROTATIONAL_OFFSET(self, widget):
        self.CALL_MDI("G10 L2 P0 R 0")
        STATUS.emit('reload-display')

    def RECORD_CURRENT_MODE(self):
        mode = STATUS.get_current_mode()
        self.last_mode = mode
        return mode

    def RESTORE_RECORDED_MODE(self):
        self.ensure_mode(self.last_mode)

    def DO_JOG(self, axisnum, direction, distance=0):
        jjogmode,j_or_a = STATUS.get_jog_info(axisnum)
        if direction == 0:
            self.cmd.jog(linuxcnc.JOG_STOP, jjogmode, j_or_a)
        else:
            if axisnum in (3,4,5):
                rate = STATUS.angular_jog_velocity
            else:
                rate = STATUS.current_jog_rate/60
            if distance == 0:
                self.cmd.jog(linuxcnc.JOG_CONTINUOUS, jjogmode, j_or_a, direction * rate)
            else:
                self.cmd.jog(linuxcnc.JOG_INCREMENT, jjogmode, j_or_a, direction * rate, distance)


    ######################################
    # Action Helper functions
    ######################################

    def ensure_mode(self, *modes):
        truth, premode = STATUS.check_for_modes(modes)
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
        STATUS = GStat()
        self.p = p
        self.stderr_text = []
        self.program_filter = program_filter
        self.callback = callback
        self.gid = STATUS.connect('periodic', self.update)
        #progress = Progress(1, 100)
        #progress.set_text(_("Filtering..."))

    def update(self, w):
        if self.p.poll() is not None:
            self.finish()
            STATUS.disconnect(self.gid)
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

