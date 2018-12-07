import linuxcnc
import hal

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
        STATUS.connect('error', lambda w, kind, text: self.record_error(kind, text))
        self.clear_last_error()

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

    def SET_LIMITS_OVERRIDE(self):
        self.cmd.override_limits()

    def SET_MDI_MODE(self):
        self.ensure_mode(linuxcnc.MODE_MDI)

    def SET_MANUAL_MODE(self):
        self.ensure_mode(linuxcnc.MODE_MANUAL)

    def CALL_MDI(self, code):
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi('%s'%code)

    def CALL_MDI_WAIT(self, code):
        log.debug('MDI_WAIT_COMMAND= {}'.format(code))
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.clear_last_error()
        for l in code.split("\n"):
            self.cmd.mdi( l )
            result = self.cmd.wait_complete()
            if result == -1 or result == linuxcnc.RCS_ERROR:
                return -1
        return 0

    def CALL_INI_MDI(self, number):
        try:
            mdi = INFO.MDI_COMMAND_LIST[number]
        except:
            log.error('MDI_COMMAND= # {} Not found under [MDI_COMMAND_LIST] in INI file'.format(number))
            return
        mdi_list = mdi.split(';')
        self.ensure_mode(linuxcnc.MODE_MDI)
        for code in(mdi_list):
            self.cmd.mdi('%s'% code)

    def CALL_OWORD(self, code):
        log.debug('OWORD_COMMAND= {}'.format(code))
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.clear_last_error()
        self.cmd.mdi(code)
        STATUS.stat.poll()
        while STATUS.stat.exec_state == linuxcnc.EXEC_WAITING_FOR_MOTION_AND_IO or \
                        STATUS.stat.exec_state == linuxcnc.EXEC_WAITING_FOR_MOTION:
            result = self.cmd.wait_complete()
            if result == -1 or result == linuxcnc.RCS_ERROR:
                log.error('MDI_COMMAND= # {}'.format(result))
                return -1
            STATUS.stat.poll()
        result = self.cmd.wait_complete()
        if result == -1 or result == linuxcnc.RCS_ERROR:
            log.error('MDI_COMMAND= # {}'.format(result))
            return -1
        log.debug('OWORD_COMMAND returns complete : {}'.format(result))
        return 0

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
        self.RELOAD_DISPLAY()

    def SET_AXIS_ORIGIN(self,axis,value):
        m = "G10 L20 P0 %s%f"%(axis,value)
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        self.RELOAD_DISPLAY()

    def RUN(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)
        if STATUS.is_auto_paused():
            self.cmd.auto(linuxcnc.AUTO_STEP)
            return
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
        STATUS.set_jograte(float(rate))
    def SET_JOG_RATE_ANGULAR(self, rate):
        STATUS.set_jograte_angular(float(rate))
    def SET_JOG_INCR(self, incr, text):
        STATUS.set_jog_increments(incr, text)
    def SET_JOG_INCR_ANGULAR(self, incr, text):
        STATUS.set_jog_increment_angular(incr, text)

    def SET_SPINDLE_ROTATION(self, direction = 1, rpm = 100):
        self.cmd.spindle(direction,rpm)
    def SET_SPINDLE_FASTER(self):
        self.cmd.spindle(linuxcnc.SPINDLE_INCREASE)
    def SET_SPINDLE_SLOWER(self):
        self.cmd.spindle(linuxcnc.SPINDLE_DECREASE)
    def SET_SPINDLE_STOP(self):
        self.cmd.spindle(linuxcnc.SPINDLE_OFF)

    def SET_USER_SYSTEM(self, system):
        systemnum = str(system).strip('gG')
        if systemnum in('54', '55', '56', '57', '58', '59', '59.1', '59.2', '59.3'):
            fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
            self.cmd.mdi('G'+systemnum)
            self.cmd.wait_complete()
            self.ensure_mode(premode)
            
    def ZERO_G92_OFFSET(self):
        self.CALL_MDI("G92.1")
        self.RELOAD_DISPLAY()
    def ZERO_ROTATIONAL_OFFSET(self):
        self.CALL_MDI("G10 L2 P0 R 0")
        self.RELOAD_DISPLAY()

    def RECORD_CURRENT_MODE(self):
        mode = STATUS.get_current_mode()
        self.last_mode = mode
        return mode

    def RESTORE_RECORDED_MODE(self):
        self.ensure_mode(self.last_mode)

    def SET_SELECTED_AXIS(self, data):
        STATUS.set_selected_axis(data)

    # jog based on STATUS's rate and distace
    def DO_JOG(self, axisnum, direction):
        if axisnum in (3,4,5):
            distance = STATUS.get_jog_increment_angular()
            rate = STATUS.get_jograte_angular()/60
        else:
            distance = STATUS.get_jog_increment()
            rate = STATUS.get_jograte()/60
        self.JOG(axisnum, direction, rate, distance)

    # jog based on given varaibles
    # checks for jog joint mode first
    def JOG(self, axisnum, direction, rate, distance=0):
        jjogmode,j_or_a = STATUS.get_jog_info(axisnum)
        if direction == 0:
            self.cmd.jog(linuxcnc.JOG_STOP, jjogmode, j_or_a)
        else:
            if distance == 0:
                self.cmd.jog(linuxcnc.JOG_CONTINUOUS, jjogmode, j_or_a, direction * rate)
            else:
                self.cmd.jog(linuxcnc.JOG_INCREMENT, jjogmode, j_or_a, direction * rate, distance)

    def TOGGLE_FLOOD(self):
        self.cmd.flood(not(STATUS.stat.flood))
    def SET_FLOOD_ON(self):
        self.cmd.flood(1)
    def SET_FLOOD_OFF(self):
        self.cmd.flood(0)

    def TOGGLE_MIST(self):
        self.cmd.mist(not(STATUS.stat.mist))
    def SET_MIST_ON(self):
        self.cmd.mist(1)
    def SET_MIST_OFF(self):
        self.cmd.mist(0)

    def RELOAD_TOOLTABLE(self):
        self.cmd.load_tool_table()                

    def TOGGLE_OPTIONAL_STOP(self):
        self.cmd.set_optional_stop(not(STATUS.stat.optional_stop))
    def SET_OPTIONAL_STOP_ON(self):
        self.cmd.set_optional_stop(True)
    def SET_OPTIONAL_STOP_OFF(self):
        self.cmd.set_optional_stop(False)

    def TOGGLE_BLOCK_DELETE(self):
        self.cmd.set_block_delete(not(STATUS.stat.block_delete))
    def SET_BLOCK_DELETE_ON(self):
        self.cmd.set_block_delete(True)
    def SET_BLOCK_DELETE_OFF(self):
        self.cmd.set_block_delete(False)

    def RELOAD_DISPLAY(self):
        STATUS.emit('reload-display')

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

    def check_error(self):
        if self._error[0] != 0:
                log.error('MDI_COMMAND_WAIT: {}'.format(self._error[1]))
                return -1
        return 0

    def record_error(self, kind, text):
        log.error('STATUS ERROR RECEIVED:')
        self._error = [kind, text]

    def clear_last_error(self):
        self._error = [0, '']

#############################
###########################################
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

