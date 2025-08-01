import os
import math
import subprocess
from time import sleep
from PyQt5.QtWidgets import (QApplication, QTabWidget, QStackedWidget,
    QWidget, QGridLayout,QGraphicsBlurEffect, QGraphicsDropShadowEffect,
                QGraphicsColorizeEffect)
from PyQt5.QtCore import Qt, QProcess

import linuxcnc

# Set up logging
from . import logger

LOG = logger.getLogger(__name__)
# LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

from qtvcp.core import Status, Info, Path

INFO = Info()
STATUS = Status()
PATH = Path()

################################################################
# Action class
################################################################
class _Lcnc_Action(object):
    TOUCHOFF_SUBPROGRAM = PATH.TOUCHOFF_SUBPROGRAM

    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
        self.__class__._instanceNum += 1
        self.cmd = linuxcnc.command()
        self.tmp = None
        self.prefilter_path = None
        self.home_all_warning_flag = False
        self.proc = None
        self.lastOriginSet = [0,0,0,0,0,0,0,0,0]

        # imported here to avoid circular imports
        from qtvcp.lib.mdi_subprogram.mdi_command_process import MDICommand
        self.MDIPROCESS = MDICommand()

    def SET_DEBUG_LEVEL(self, level):
        self.cmd.debug(level)

    def SET_ESTOP_STATE(self, state):
        if isinstance(state, bool):
            if state:
                self.cmd.state(linuxcnc.STATE_ESTOP)
            else:
                self.cmd.state(linuxcnc.STATE_ESTOP_RESET)
        elif state in (state,linuxcnc.STATE_ESTOP,linuxcnc.STATE_ESTOP_RESET):
            self.cmd.state(state)

    def SET_MACHINE_STATE(self, state):
        if isinstance(state, bool):
            if state:
                self.cmd.state(linuxcnc.STATE_ON)
            else:
                self.cmd.state(linuxcnc.STATE_OFF)
        elif state in (state,linuxcnc.STATE_ON,linuxcnc.STATE_ESTOP_OFF):
            self.cmd.state(state)

    def TOGGLE_TELEOP_MODE(self):
        self.SET_MOTION_TELEOP(not (STATUS.is_world_mode()))

    def SET_MOTION_TELEOP(self, value):
        # 1:teleop, 0: joint
        #if value:
        #    print('To telop (1)')
        #else:
        #    print('To joint (0)')

        self.cmd.teleop_enable(value)
        self.cmd.wait_complete()
        STATUS.stat.poll()

    def SET_MACHINE_HOMING(self, joint):
        self.ensure_mode(linuxcnc.MODE_MANUAL)
        self.cmd.teleop_enable(False)
        if not INFO.HOME_ALL_FLAG and joint == -1:
            if not self.home_all_warning_flag == True:
                self.home_all_warning_flag = True
                STATUS.emit('error', linuxcnc.NML_ERROR,
                            ''''Home-all not available according to INI Joint Home sequence
                            Set the joint sequence in the INI or
                            modify the screen for individual home buttons
                            to avoid this warning
                            Press again to home Z axis Joint''')
            else:
                if STATUS.is_all_homed():
                    self.home_all_warning_flag = False
                    return
                # so linuxcnc is misonfigured or the Screen is built wrong (needs individual home buttons)
                # now we will fake individual home buttons by homing joints one at a time
                # but always start will Z - on a mill it's safer
                zj = INFO.GET_JOG_FROM_NAME['Z']
                if not STATUS.stat.homed[zj]:
                    LOG.info('Homing Joint: {}'.format(zj))
                    self.cmd.home(zj)
                    STATUS.emit('error', linuxcnc.NML_ERROR,
                                ''''Home-all not available according to INI Joint Home sequence
             Press again to home next Joint''')
                    return
                length = len(INFO.JOINT_SEQUENCE_LIST)
                for num, j in enumerate(INFO.JOINT_SEQUENCE_LIST):
                    #print(j, num, len(INFO.JOINT_SEQUENCE_LIST))
                    # at the end so all homed
                    if num == length - 1:
                        self.home_all_warning_flag = False
                    # one from end but end is already homed
                    if num == length - 2 and STATUS.stat.homed[zj]:
                        self.home_all_warning_flag = False
                    # Z joint is homed first outside this for loop
                    if j == zj: continue
                    # ok home it then stop and wait for next button push
                    if not STATUS.stat.homed[j]:
                        LOG.info('Homing Joint: {}'.format(j))
                        self.cmd.home(j)
                        if self.home_all_warning_flag:
                            STATUS.emit('error', linuxcnc.NML_ERROR,
                                        ''''Home-all not available according to INI Joint Home sequence
                     Press again to home next Joint''')
                        break
        else:
            LOG.info('Homing Joint: {}'.format(joint))
            self.cmd.home(joint)

    def SET_MACHINE_UNHOMED(self, joint):
        self.ensure_mode(linuxcnc.MODE_MANUAL)
        self.cmd.teleop_enable(False)

        if joint < 0:
            # unhome all joints
            self.cmd.unhome(joint)
        else:
            # if you unhome a joint that is combined with another (to make an axis)
            # then unhome both.
            for j in (INFO.JOINT_RELATIONS_LIST[joint]):
                self.cmd.unhome(j)

    def SET_AUTO_MODE(self):
        self.ensure_mode(linuxcnc.MODE_AUTO)

    # if called while on hard limit will set the flag and allow machine on
    # if called with flag set and now off hard limits - resets the flag
    def TOGGLE_LIMITS_OVERRIDE(self):
        if STATUS.is_limits_override_set() and STATUS.is_hard_limits_tripped():
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '''Can Not Reset Limits Override - Still On Hard Limits''')
            # let calling function know we didn't release the limit override
            return False
        elif not STATUS.is_limits_override_set() and STATUS.is_hard_limits_tripped():
            STATUS.emit('error', STATUS.TEMPORARY_MESSAGE, 'Hard Limits Are Overridden!')
            self.cmd.override_limits()
        else:
            STATUS.emit('error', STATUS.TEMPORARY_MESSAGE, 'Hard Limits Are Reset To Active!')
            self.cmd.override_limits()

    def SET_MDI_MODE(self):
        self.ensure_mode(linuxcnc.MODE_MDI)

    def SET_MANUAL_MODE(self):
        self.ensure_mode(linuxcnc.MODE_MANUAL)

    # sets up a python generator that goes through the MDI list of lists.
    # if it's a command that we have to wait indefinitely
    # ie like a manual tool change.
    # then we wait for STATUS to return 'command-stopped'
    # and then continue where we left off.
    # normal commands just call normal mdi
    # when the generator ends it forces the return
    # to the recorded mode.
    def CALL_MDI_LIST(self, code):
        self.RECORD_CURRENT_MODE()
        self.ensure_mode(linuxcnc.MODE_MDI)
        self._gen = self.generate_list(code)
        try:
            state = next(self._gen)
        except StopIteration:
            pass

    def CALL_MDI(self, code):
        LOG.debug('CALL_MDI Command: {}'.format(code))
        if STATUS.is_auto_running():
            LOG.error('Can not run MDI command:{} when linuxcnc is running in auto mode'.format(code))
            return -1
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi('%s' % code)
        return 1

    def CALL_BACKGROUND_MDI(self, code, label='Background MDI',timeout=30):
        LOG.debug('CALL_MDI Command: {} called {}'.format(label, code))
        if STATUS.is_auto_running():
            LOG.error('Can not run MDI command:{} when linuxcnc is running in auto mode'.format(code))
            return -1
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.MDIPROCESS.run(cmdList={'LABEL':label,'COMMANDS':code,'TIMEOUT':timeout})
        return 1

    def CALL_MDI_WAIT(self, code, time=5, mode_return=False):
        LOG.debug('MDI_WAIT_Command= {}, maxt = {}'.format(code, time))
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        for l in code.split("\n"):
            LOG.debug('CALL_MDI_WAIT Command: {}'.format(l))
            self.cmd.mdi(l)
            result = self.cmd.wait_complete(time)
            if result == -1:
                LOG.error('CALL_MDI_WAIT timeout surpassed {} seconds'.format(time))
                # STATUS.emit('MDI time out error',)
                self.ABORT()
                return -1
            elif result == linuxcnc.RCS_ERROR:
                LOG.debug('CALL_MDI_WAIT RCS error: {}'.format(time, result))
                return -1
            sleep(.5)
            error = STATUS.ERROR.poll()
            # next commented line just for debugging
            #self.cmd.error_msg('returned:'+str(error))
            if error:
                STATUS.emit('error', error[0], error[1])
                LOG.error('CALL_MDI_WAIT Error: {}'.format(error[1]))
                return -1
        if mode_return:
            self.ensure_mode(premode)
        return 0

    def CALL_INI_MDI(self, key):
        try:
            # prefer named INI MDI commands
            mdi = INFO.get_ini_mdi_command(key)
            LOG.debug('COMMAND= {}'.format(mdi))
            if mdi is None: raise Exception
        except:
            # fallback to legacy nth line
            try:
                mdi = INFO.MDI_COMMAND_LIST[key]
            except:
                msg = 'MDI_COMMAND_{} Not found under [MDI_COMMAND_LIST] in INI file'.format(key)
                LOG.error(msg)
                self.SET_ERROR_MESSAGE(msg)
                return

        mdi_list = mdi.split(';')
        self.ensure_mode(linuxcnc.MODE_MDI)
        for code in (mdi_list):
            LOG.debug('CALL_INI_MDI command:{}'.format(code))
            self.cmd.mdi('%s' % code)

    def CALL_OWORD(self, code, time=5):
        LOG.debug('OWORD_COMMAND= {}'.format(code))
        self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(code)
        STATUS.stat.poll()
        while STATUS.stat.exec_state == linuxcnc.EXEC_WAITING_FOR_MOTION_AND_IO or \
                STATUS.stat.exec_state == linuxcnc.EXEC_WAITING_FOR_MOTION:
            result = self.cmd.wait_complete(time)
            if result == -1:
                LOG.error('Oword timeout oast () Error = # {}'.format(time, result))
                self.ABORT()
                return -1
            elif result == linuxcnc.RCS_ERROR:
                LOG.error('Oword RCS Error = # {}'.format(result))
                return -1
            result = linuxcnc.error_channel().poll()
            if result:
                STATUS.emit('error', result[0], result[1])
                LOG.error('Oword Error: {}'.format(result[1]))
                return -1
            STATUS.stat.poll()
        result = self.cmd.wait_complete(time)
        if result == -1 or result == linuxcnc.RCS_ERROR or linuxcnc.error_channel().poll():
            LOG.error('Oword RCS Error = # {}'.format(result))
            return -1
        result = linuxcnc.error_channel().poll()
        if result:
            STATUS.emit('error', result[0], result[1])
            LOG.error('Oword Error: {}'.format(result[1]))
            return -1
        LOG.debug('OWORD_COMMAND returns complete : {}'.format(result))
        return 0

    def UPDATE_VAR_FILE(self):
        self.ensure_mode(linuxcnc.MODE_MANUAL)
        self.ensure_mode(linuxcnc.MODE_MDI)

    def OPEN_PROGRAM(self, fname):
        self.prefilter_path = str(fname)
        old = STATUS.stat.file
        flt = INFO.get_filter_program(str(fname))

        # probably not needed now but in case wanted
        if INFO.INI.find("DISPLAY", "NO_MULTIPLE_DOT_FILENAME"):
            if os.path.basename(fname).count('.') > 1:
                e = 'Open File error: Multiple \'.\' not allowed in Linuxcnc'
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, e)
                LOG.debug(e)
                return

        if flt:
            LOG.debug('get {} filtered program {}'.format(flt, fname))
            self.open_filter_program(str(fname), flt)
        else:
            LOG.debug('Load program {}'.format(fname))
            self.cmd.program_open(str(fname))

            # STATUS can't tell if we are loading the same file.
            # for instance if you edit a file then load it again.
            # so we check for it and force an update:
            if old == fname:
                STATUS.emit('file-loaded', fname)

    def SAVE_PROGRAM(self, source, fname, ending = '.ngc'):
        # no gcode - ignore
        if source == '':
            return None

        npath = None
        # normalize to absolute path
        try:
            path = os.path.abspath(fname)
            name, ext = os.path.splitext(path)

            # add extension if missing
            if ext == '':
                ext = ending
            npath = name + ext.lower()

            # might not need this now but here it is in case wanted
            if INFO.INI.find("DISPLAY", "NO_MULTIPLE_DOT_FILENAME"):
                if npath.count('.') > 1:
                    e = 'Save Error: Multiple \'.\' not allowed in Linuxcnc'
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, e)
                    LOG.debug(e)
                    return None
        except Exception as e:
            LOG.debug('save error: {}'.format(e))
            LOG.debug('Original save path: {}'.format(fname))

        LOG.debug('SAVE_PROGRAM write to: {}'.format(npath))

        # ok write the file
        outfile = None
        try:
            outfile = open(npath, 'w')
            outfile.write(source)
            STATUS.emit('update-machine-log', 'Saved: ' + npath, 'TIME,SUCCESS')
        except Exception as e:
            print(e)
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, e)
            try:
                outfile.close()
            except:
                pass
            return None
        finally:
            try:
                outfile.close()
            except:
                pass
            return npath

    def SET_AXIS_ORIGIN(self, axis, value):
        if axis == '' or axis.upper() not in ("XYZABCUVW"):
            LOG.warning("Couldn't set origin -axis >{}< not recognized:".format(axis))
            return

        # record current setting
        j = "XYZABCUVW"
        jnum = j.find(axis)
        p,r,d = STATUS.get_position()
        if STATUS.is_metric_mode() != INFO.MACHINE_IS_METRIC:
            r = INFO.convert_units_9(r)
        self.lastOriginSet[jnum] = r[jnum]

        # set new position
        m = "G10 L20 P0 %s%f" % (axis, value)
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        self.RELOAD_DISPLAY()

    def GET_LAST_RECORDED_ORIGIN(self, axis):
        j = "XYZABCUVW"
        jnum = j.find(axis)
        return  self.lastOriginSet[jnum]

    # Adjust tool offsets so current position ends up the given value
    def SET_TOOL_OFFSET(self, axis, value, fixture=False):
        lnum = 10 + int(fixture)
        m = "G10 L%d P%d %s%f" % (lnum, STATUS.stat.tool_in_spindle, axis, value)
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.cmd.mdi("G43")
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        self.RELOAD_DISPLAY()

    # Set actual tool offset in tool table to the given value
    def SET_DIRECT_TOOL_OFFSET(self, axis, value):
        m = "G10 L1 P%d %s%f" % (STATUS.get_current_tool(), axis, value)
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.cmd.mdi("G43")
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        self.RELOAD_DISPLAY()

    def RUN(self, line=0):
        if not STATUS.is_auto_mode():
            self.ensure_mode(linuxcnc.MODE_AUTO)
        if STATUS.is_auto_paused() and line == 0:
            self.cmd.auto(linuxcnc.AUTO_STEP)
            return
        elif not STATUS.is_auto_running():
            self.cmd.auto(linuxcnc.AUTO_RUN, line)

    def STEP(self):
        if STATUS.is_auto_running() and not STATUS.is_auto_paused():
            self.cmd.auto(linuxcnc.AUTO_PAUSE)
            return
        if STATUS.is_auto_paused():
            self.cmd.auto(linuxcnc.AUTO_STEP)
            return

    def ABORT(self):
        self.cmd.abort()

    # much used Legacy code
    def PAUSE(self):
        if not STATUS.stat.paused:
            self.cmd.auto(linuxcnc.AUTO_PAUSE)
        else:
            LOG.debug('resume')
            self.cmd.auto(linuxcnc.AUTO_RESUME)

    def TOGGLE_PAUSE(self):
        if not STATUS.stat.paused:
            self.cmd.auto(linuxcnc.AUTO_PAUSE)
        else:
            self.cmd.auto(linuxcnc.AUTO_RESUME)

    def PAUSE_MACHINE(self):
        if not STATUS.stat.paused:
            self.cmd.auto(linuxcnc.AUTO_PAUSE)

    def RESUME(self):
        if STATUS.stat.paused:
            self.cmd.auto(linuxcnc.AUTO_RESUME)

    def SET_MAX_VELOCITY_RATE(self, rate):
        self.cmd.maxvel(rate / 60.0)

    def SET_RAPID_RATE(self, rate):
        self.cmd.rapidrate(rate / 100.0)

    def SET_FEED_RATE(self, rate):
        self.cmd.feedrate(rate / 100.0)

    def SET_SPINDLE_RATE(self, rate, number=0):
        self.cmd.spindleoverride(rate / 100.0, number)

    # machine units per minute
    def SET_JOG_RATE(self, rate):
        STATUS.set_jograte(float(rate))

    # keyboard shortcut uses it
    def SET_JOG_RATE_FASTER(self, divs=30):
        nrate = self._step_jograte(STATUS.get_jograte(),
            INFO.MIN_LINEAR_JOG_VEL, INFO.MAX_LINEAR_JOG_VEL, 1, divs)
        STATUS.set_jograte(nrate)

    # keyboard shortcut uses it
    def SET_JOG_RATE_SLOWER(self, divs=30):
        nrate = self._step_jograte(STATUS.get_jograte(),
            INFO.MIN_LINEAR_JOG_VEL, INFO.MAX_LINEAR_JOG_VEL, -1, divs)
        STATUS.set_jograte(nrate)

    # degrees per minute
    def SET_JOG_RATE_ANGULAR(self, rate):
        STATUS.set_jograte_angular(float(rate))

    # keyboard shortcut uses it
    def SET_JOG_RATE_ANGULAR_FASTER(self, divs=30):
        nrate = self._step_jograte(STATUS.get_jograte_angular(),
            INFO.MIN_ANGULAR_JOG_VEL, INFO.MAX_ANGULAR_JOG_VEL, 1, divs)
        STATUS.set_jograte_angular(float(nrate))

    # keyboard shortcut uses it
    def SET_JOG_RATE_ANGULAR_SLOWER(self, divs=30):
        nrate = self._step_jograte(STATUS.get_jograte_angular(),
            INFO.MIN_ANGULAR_JOG_VEL, INFO.MAX_ANGULAR_JOG_VEL, -1, divs)
        STATUS.set_jograte_angular(float(nrate))

    def SET_JOG_INCR(self, incr, text):
        STATUS.set_jog_increments(incr, text)
        # stop runaway jogging
        for jnum in range(STATUS.stat.joints):
            self.STOP_JOG(jnum)

    def SET_JOG_INCR_ANGULAR(self, incr, text):
        STATUS.set_jog_increment_angular(incr, text)
        # stop runaway joging
        for jnum in range(STATUS.stat.joints):
            self.STOP_JOG(jnum)

    def SET_SPINDLE_ROTATION(self, direction=1, rpm=100, number=-1):
        self.cmd.spindle(direction, rpm, number)

    def SET_SPINDLE_FASTER(self, number=0):
        # if all spindles (-1) command , we must check each spindle
        if number == -1:
            a = 0
            b = INFO.AVAILABLE_SPINDLES
        else:
            a = number
            b = number + 1
        for i in range(a, b):
            cur = STATUS.get_spindle_speed(i)
            if cur > 0:
                dir = 1
            else:
                dir = -1
            if abs(cur + (INFO.SPINDLE_INCREMENT * dir)) >= INFO['MAX_SPINDLE_{}_SPEED'.format(i)]:
                self.cmd.spindle(dir, INFO['MAX_SPINDLE_{}_SPEED'.format(i)], i)
                continue
            else:
                self.cmd.spindle(dir, abs(cur + (INFO.SPINDLE_INCREMENT * dir)), i)

    def SET_SPINDLE_SLOWER(self, number=0):
        # if all spindles (-1) command , we must check each spindle
        if number == -1:
            a = 0
            b = INFO.AVAILABLE_SPINDLES
        else:
            a = number
            b = number + 1
        for i in range(a, b):
            cur = STATUS.get_spindle_speed(i)
            if cur > 0:
                dir = 1
            else:
                dir = -1
            if abs(cur - (INFO.SPINDLE_INCREMENT * dir)) <= INFO['MIN_SPINDLE_{}_SPEED'.format(i)]:
                self.cmd.spindle(dir, INFO['MIN_SPINDLE_{}_SPEED'.format(i)], i)
                continue
            else:
                self.cmd.spindle(dir, abs(cur - (INFO.SPINDLE_INCREMENT * dir)), i)

    def SET_SPINDLE_STOP(self, number=0):
        self.cmd.spindle(linuxcnc.SPINDLE_OFF, number)

    def SET_USER_SYSTEM(self, system):
        systemnum = str(system).strip('gG')
        if systemnum in ('54', '55', '56', '57', '58', '59', '59.1', '59.2', '59.3'):
            fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
            self.cmd.mdi('G' + systemnum)
            self.cmd.wait_complete()
            self.ensure_mode(premode)

    def ZERO_G92_OFFSET(self):
        self.CALL_MDI("G92.1")
        self.RELOAD_DISPLAY()

    def ZERO_ROTATIONAL_OFFSET(self):
        self.CALL_MDI("G10 L2 P0 R 0")
        self.RELOAD_DISPLAY()

    def ZERO_G5X_OFFSET(self, num):
        fail, premode = self.ensure_mode(linuxcnc.MODE_MDI)
        clear_command = "G10 L2 P%d R0" % num
        for a in INFO.AVAILABLE_AXES:
            clear_command += " %c0" % a
        self.cmd.mdi('%s' % clear_command)
        self.cmd.wait_complete()
        self.ensure_mode(premode)
        self.RELOAD_DISPLAY()

    def RECORD_CURRENT_MODE(self):
        mode = STATUS.get_current_mode()
        self.last_mode = mode
        return mode

    def RESTORE_RECORDED_MODE(self):
        self.cmd.wait_complete()
        self.ensure_mode(self.last_mode)

    def SET_SELECTED_JOINT(self, data):
        if isinstance(data, int):
            STATUS.set_selected_joint(data)
        else:
            LOG.error('Selected joint must be an integer: {}'.format(data))

    def SET_SELECTED_AXIS(self, data):
        if isinstance(data, (str)):
            STATUS.set_selected_axis(data)
        else:
            LOG.error('Selected axis must be a string: {}'.format(data))

    # jog based on STATUS's rate and distance
    # use joint number for joint or letter for axis jogging
    def DO_JOG(self, joint_axis, direction):
        angular = False
        if isinstance(joint_axis, int):
            if STATUS.stat.joint[joint_axis]['jointType'] == linuxcnc.ANGULAR:
                angular = True
            jointnum = joint_axis
        else:
            if joint_axis.upper() in ('A', 'B', 'C'):
                angular = True
            s = 'XYZABCUVW'
            jointnum = s.find(joint_axis)
        # Get jog rate
        if angular:
            distance = STATUS.get_jog_increment_angular()
            rate = STATUS.get_jograte_angular() / 60
        else:
            distance = STATUS.get_jog_increment()
            rate = STATUS.get_jograte() / 60
        self.JOG(jointnum, direction, rate, distance)

    # jog based on given variables
    # checks for jog joint mode first
    def JOG(self, jointnum, direction, rate, distance=0):
        jjogmode, j_or_a = self.get_jog_info(jointnum)
        if jjogmode is None or j_or_a is None:
            return
        if direction == 0:
            self.cmd.jog(linuxcnc.JOG_STOP, jjogmode, j_or_a)
        else:
            if distance == 0:
                self.cmd.jog(linuxcnc.JOG_CONTINUOUS, jjogmode, j_or_a, direction * rate)
            else:
                self.cmd.jog(linuxcnc.JOG_INCREMENT, jjogmode, j_or_a, direction * rate, distance)

    def STOP_JOG(self, jointnum):
        if STATUS.machine_is_on() and STATUS.is_man_mode():
            jjogmode, j_or_a = self.get_jog_info(jointnum)
            self.cmd.jog(linuxcnc.JOG_STOP, jjogmode, j_or_a)

    def TOGGLE_FLOOD(self):
        self.cmd.flood(not (STATUS.stat.flood))

    def SET_FLOOD_ON(self):
        self.cmd.flood(1)

    def SET_FLOOD_OFF(self):
        self.cmd.flood(0)

    def TOGGLE_MIST(self):
        self.cmd.mist(not (STATUS.stat.mist))

    def SET_MIST_ON(self):
        self.cmd.mist(1)

    def SET_MIST_OFF(self):
        self.cmd.mist(0)

    def RELOAD_TOOLTABLE(self):
        self.cmd.load_tool_table()

    def TOGGLE_OPTIONAL_STOP(self):
        self.cmd.set_optional_stop(not (STATUS.stat.optional_stop))

    def SET_OPTIONAL_STOP_ON(self):
        self.cmd.set_optional_stop(True)

    def SET_OPTIONAL_STOP_OFF(self):
        self.cmd.set_optional_stop(False)

    def TOGGLE_BLOCK_DELETE(self):
        self.cmd.set_block_delete(not (STATUS.stat.block_delete))

    def SET_BLOCK_DELETE_ON(self):
        self.cmd.set_block_delete(True)

    def SET_BLOCK_DELETE_OFF(self):
        self.cmd.set_block_delete(False)

    def RELOAD_DISPLAY(self):
        STATUS.emit('reload-display')

    def SET_GRAPHICS_VIEW(self, view):
        if view.lower() in ('x', 'y', 'y2', 'z', 'z2', 'p', 'clear',
                            'zoom-in', 'zoom-out', 'pan-up', 'pan-down',
                            'pan-left', 'pan-right', 'rotate-up',
                            'rotate-down', 'rotate-cw', 'rotate-ccw',
                            'overlay-dro-on', 'overlay-dro-off',
                            'dtg-on', 'dtg-off',
                            'overlay-offsets-on', 'overlay-offsets-off',
                            'inhibit-selection-on', 'inhibit-selection-off',
                            'alpha-mode-on', 'alpha-mode-off', 'dimensions-on',
                            'dimensions-off', 'record-view', 'set-recorded-view',
                            'set-large-dro','set-small-dro',"grid-off"):
            STATUS.emit('graphics-view-changed', view, None)

    def SET_GRAPHICS_GRID_COLOR(self, color):
        STATUS.emit('graphics-view-changed', 'GRID-COLOR', {'COLOR': color})

    def SET_GRAPHICS_GRID_SIZE(self, size):
        STATUS.emit('graphics-view-changed', 'GRID-SIZE', {'SIZE': size})

    def SET_GRAPHICS_SCROLL_MODE(self, mode):
        STATUS.emit('graphics-view-changed', 'SCROLL-MODE', {'MODE': mode})


    def ADJUST_GRAPHICS_PAN(self, x, y):
        STATUS.emit('graphics-view-changed', 'pan-view', {'X': x, 'Y': y})

    def ADJUST_GRAPHICS_ROTATE(self, x, y):
        STATUS.emit('graphics-view-changed', 'rotate-view', {'X': x, 'Y': y})

    def SHUT_SYSTEM_DOWN_PROMPT(self):
        import subprocess
        import shutil
        try:
            if shutil.which('gnome-session-quit'):
                subprocess.call('gnome-session-quit --power-off', shell=True)
            elif shutil.which('xfce4-session-logout'):
                subprocess.call('xfce4-session-logout', shell=True)
            else:
                subprocess.call('systemctl poweroff', shell=True)
        except Exception as e:
            LOG.warning("Couldn't shut system down: {}".format(e))

    def SHUT_SYSTEM_DOWN_NOW(self):
        import subprocess
        subprocess.call('shutdown now')

    def UPDATE_MACHINE_LOG(self, text, option=None):
        valid_options = {'INITIAL', 'TIME', 'DATE', 'DELETE', 'CRITICAL', 'ERROR', 'WARNING', 'SUCCESS', 'DEBUG'}
        options = set(option.split(',')) if option else {None}
        
        if not options.issubset(valid_options):
            invalid_options = options - valid_options
            LOG.warning("Machine_log option(s) not recognized: {}".format(', '.join(invalid_options)))
            options = None
    
        STATUS.emit('update-machine-log', text, options)

    def CALL_DIALOG(self, command):
        try:
            a = command['NAME']
        except:
            LOG.warning("Call Dialog command Dict not recogzied: {}".format(command))
        STATUS.emit('dialog-request', command)

    def HIDE_POINTER(self, state):
        if state:
            QApplication.setOverrideCursor(Qt.BlankCursor)
        else:
            QApplication.restoreOverrideCursor()

    def PLAY_SOUND(self, path):
        try:
            STATUS.emit('play-sound', path)
        except AttributeError:
            LOG.warning("Sound request {} not recogzied".format(path))

    def PLAY_ERROR(self):
        self.PLAY_SOUND('ERROR')

    def PLAY_DONE(self):
        self.PLAY_SOUND('DONE')

    def PLAY_READY(self):
        self.PLAY_SOUND('READY')

    def PLAY_ATTENTION(self):
        self.PLAY_SOUND('ATTENTION')

    def PLAY_LOGIN(self):
        self.PLAY_SOUND('LOGIN')

    def PLAY_LOGOUT(self):
        self.PLAY_SOUND('LOGOUT')

    def SPEAK(self, speech):
        STATUS.emit('play-sound', 'SPEAK {}'.format(speech))

    def BEEP(self):
        self.PLAY_SOUND('BEEP')

    def BEEP_RING(self):
        self.PLAY_SOUND('BEEP_RING')

    def BEEP_START(self):
        self.PLAY_SOUND('BEEP_START')

    def SET_LATHE_MIRROR_X(self):
        if not INFO.MACHINE_IS_LATHE:
            LOG.warning('Can not set mirror mode; Machine is not a lathe')
            return
        self.CALL_MDI("G10 L2 P0 R180")
        self.RELOAD_DISPLAY()

    def UNSET_LATHE_MIRROR_X(self):
        if not INFO.MACHINE_IS_LATHE:
            LOG.warning('Can not unset mirror mode; Machine is not a lathe')
            return
        self.CALL_MDI("G10 L2 P0 R0")
        self.RELOAD_DISPLAY()

    # Some systems need repeat disabled for keyboard jogging because repeat rate is uneven
    def DISABLE_AUTOREPEAT_KEYS(self, keys={'34','35','80','81','83','85','88','89','111','112','113','114','116','117'}):
        for k in keys:
            subprocess.Popen('xset -r {}'.format(k), stdout = subprocess.PIPE, shell = True)

    def ENABLE_AUTOREPEAT_KEYS(self, keys={'34','35','80','81','83','85','88','89','111','112','113','114','116','117'}):
        for k in keys:
            subprocess.Popen('xset r {}'.format(k), stdout = subprocess.PIPE, shell = True)

    # send an operator info message to the gui
    def SET_DISPLAY_MESSAGE(self, msg):
        self.cmd.display_msg(msg)

    # send an operator error message to the gui
    def SET_ERROR_MESSAGE(self, msg):
        self.cmd.error_msg(msg)

    # TODO remove in future
    def SET_TEMPARARY_MESSAGE(self, msg):
        STATUS.emit('error', STATUS.TEMPORARY_MESSAGE, msg)

    def SET_TEMPORARY_MESSAGE(self, msg):
        STATUS.emit('error', STATUS.TEMPORARY_MESSAGE, msg)

    def TOUCHPLATE_TOUCHOFF(self, search_vel, probe_vel, max_probe,
            z_offset, retract_distance, z_safe_travel, rtn_method=None, error_rtn=None):
        # if not none will be called with returned data
        self._touchoff_return = rtn_method
        self._touchoff_error_return = error_rtn

        if not self.proc is None:
            return "Touchoff routine is already running"
        if not os.path.exists(self.TOUCHOFF_SUBPROGRAM):
            return "Touchoff subroutine path not found at:{}".format(self.TOUCHOFF_SUBPROGRAM)
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.touchoff_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.touchoff_finished)
        self.proc.start('python3 {}'.format(self.TOUCHOFF_SUBPROGRAM))
        # probe
        string_to_send = "touchoff${}${}${}${}${}${}\n".format(str(search_vel),
                                        str(probe_vel),
                                        str(max_probe),
                                        str(retract_distance),
                                        str(z_safe_travel),
                                        str(z_offset))
        #print(string_to_send)
        # block polling here, the sub program will poll now
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))
        return 1

    def AUTO_HEIGHT(self, string_to_send, rtn_method=None, error_rtn=None):
        # if not None, return with returned data
        self._touchoff_return = rtn_method
        self._touchoff_error_return = error_rtn

        if self.proc is not None:
            return 0
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.touchoff_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.touchoff_finished)
        self.proc.start('python3 {}'.format(self.TOUCHOFF_SUBPROGRAM))
        # block polling here, the sub program will poll now
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))
        return 1

    def ADD_WIDGET_TO_TAB(self, widgetTo, widget,name):
        try:
            if isinstance(widgetTo, QTabWidget):
                tw = QWidget()
                widgetTo.addTab(tw, name)
            elif isinstance(widgetTo, QStackedWidget):
                tw = QWidget()
                widgetTo.setMinimumWidth(widget.minimumWidth())
                widgetTo.setMaximumWidth(widget.maximumWidth())
                widgetTo.addWidget(tw)
            else:
                LOG.warning('Widget {} is not a Tab or stacked Widget - skipping'.format(widgetTo))
                return False
        except Exception as e:
            LOG.warning("problem inserting child into location: {},{}".format(widget,e))
            return False

        layout = QGridLayout(tw)
        layout.setContentsMargins(0,0,0,0)
        layout.addWidget(widget, 0, 0)

        return True

    def SET_BLUR(self, widget, state, radius = 15):
        if state:
            blur = QGraphicsBlurEffect()
            blur.setBlurRadius(radius)
            widget.setGraphicsEffect(blur)
            widget.hide()
            widget.show()
        else:
            widget.setGraphicsEffect(None)

    def SET_TINT(self, widget, state, color):
        if state:
            c = QGraphicsColorizeEffect()
            c.setColor(color)
            c.setStrength(1)
            widget.setGraphicsEffect(c)
            widget.hide()
            widget.show()
        else:
            widget.setGraphicsEffect(None)

    def SET_SHADOW(self, widget,state,color):
        if state:
            shadow = QGraphicsDropShadowEffect()
            shadow.setBlurRadius(30)
            shadow.setColor(color)
            shadow.setOffset(10, 10)
            widget.setGraphicsEffect(shadow)
            widget.hide()
            widget.show()
        else:
            widget.setGraphicsEffect(None)

    # search for INFO or README file
    def GET_ABOUT_INFO(self):
        mess = ''
        path = PATH.ABOUT
        if not os.path.exists(path):
            path = os.path.join(PATH.CONFIGPATH, 'README')
            if not os.path.exists(path):
                return "This is a Pyqt5/QtVCP based screen for Linuxcnc\n No ABOUT or README found."

        for line in open(path):
            mess += line
        return mess

    ######################################
    # Action Helper functions
    ######################################

    # adjust the jog rate by one approximate division of the
    # min/max range on an exponential scale.
    # cut off at the upper and lower jog rates as per the INI
    def _step_jograte(self, jograte, minrate, maxrate, inc, divs):
        rate = jograte - minrate
        if rate < 0:
            rate = 0
        rate = math.log(rate + 1)
        one = math.log(maxrate - minrate + 1) / divs
        now = round(rate/one)
        nrate = int(math.exp((now + inc) * one)) + minrate + 1
        if nrate > int(maxrate):
            nrate = int(maxrate)
        if nrate < int(minrate):
            nrate = int(minrate)
        return float(nrate)

    # In free (joint) mode we use the plain joint number.
    # In axis mode we convert the joint number to the equivalent
    # axis number
    def get_jog_info(self, num):
        if STATUS.stat.motion_mode == linuxcnc.TRAJ_MODE_FREE:
            return True, self.jnum_check(num)
        return False, num

    def jnum_check(self, num):
        if STATUS.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            LOG.warning("Joint jogging not supported for non-identity kinematics")
            # return None
        if num > INFO.JOINT_COUNT:
            LOG.error("Computed joint number={} exceeds jointcount={}".format(num, INFO.JOINT_COUNT))
            # decline to jog
            return None
        if num not in INFO.AVAILABLE_JOINTS:
            LOG.warning("Joint {} is not in available joints {}".format(num, INFO.AVAILABLE_JOINTS))
            return None
        return num

    # check and if required set the machine mode
    # return: state changed?, the original mode
    def ensure_mode(self, *modes):
        truth, premode = STATUS.check_for_modes(modes)
        if truth is False:
            self.cmd.mode(modes[0])
            self.cmd.wait_complete()
            return (True, premode)
        else:
            return (truth, premode)

    #------- gcode filter program

    def open_filter_program(self, fname, flt):
        LOG.debug('Opening filtering program yellow<{}> for {}'.format(flt, fname))
        if not self.tmp:
            self._mktemp()
        tmp = os.path.join(self.tmp, os.path.basename(fname))
        flt = FilterProgram(flt, fname, tmp, lambda r: r or self._load_filter_result(tmp))

    def _load_filter_result(self, fname):
        old = STATUS.stat.file
        LOG.debug('Load filtered program {}'.format(fname))
        self.cmd.program_open(str(fname))

        # STATUS can't tell if we are loading the same file.
        # for instance if you edit a file then load it again.
        # so we check for it and force an update:
        if old == fname:
            STATUS.emit('file-loaded', fname)

    def _mktemp(self):
        if self.tmp:
            return
        self.tmp = tempfile.mkdtemp(prefix='emcflt-', suffix='.d')
        atexit.register(lambda: shutil.rmtree(self.tmp))


    #-------MDI call list helpers----------

    def change_mode_after(self, gen):
        self._a = STATUS.connect('command-stopped', lambda w: self.command_stopped(gen))
    # when command stops - we try to continue the generator.
    # if generator is done - return to recorded mode.
    def command_stopped(self, gen):
        try:
            state = next(gen)
        except StopIteration:
            STATUS.handler_disconnect(self._a)

    # python generator that goes through the MDI list.
    # if it's a command that we have to wait indefinitely
    # ie like a manual tool change.
    # then we wait for STATUS to return 'command-stopped'
    # and then continue where we left off.
    # normal commands just call normal mdi
    # when the generator ends it forces the return
    # to the recorded mode.
    def generate_list(self,cmdList):
        for calltype, cmd in cmdList:
            if calltype == 'commandStatusWait':
                self.change_mode_after(self._gen)
                self.cmd.mdi('%s' % cmd)
                yield cmd
            else:
                result = self.CALL_MDI_WAIT(cmd,mode_return=False)
                if result == -1:
                    LOG.debug('MDI command {} failed.'.format(cmd))
        self.RESTORE_RECORDED_MODE()


    #------- Touch plate touchoff

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data()
        self.parse_line(line)

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data()
        self.parse_line(line)

    def parse_line(self, line):
        line = line.decode("utf-8")
        if "COMPLETE" in line:
            # did we get a return method to send return data to?
            if self._touchoff_return is None:
                self.SET_DISPLAY_MESSAGE("Touchoff routine returned successfully")
            else:
                # strip ugly text
                s = line[line.find('COMPLETE')+9:]
                self._touchoff_return(s)

        # This also gets error text sent from logging of ACTION library in the subprogram
        elif "ERROR" in line:
            # remove preceding text 'ERROR'
            s = line[line.find('ERROR')+6:]
            s = s[s.find(']')+1:]
            if self._touchoff_error_return is None:
                self.SET_ERROR_MESSAGE(s)
            else:
                self._touchoff_error_return(s)

        elif "DEBUG" in line: # must set DEBUG level on LOG in top of this file
            LOG.debug(line[line.find('DEBUG')+6:])

    def touchoff_started(self):
        LOG.debug("TouchOff subprogram started with PID {}\n".format(self.proc.processId()))

    def touchoff_finished(self, exitCode, exitStatus):
        LOG.debug("Touchoff Process finished - exitCode {} exitStatus {}".format(exitCode, exitStatus))
        self.proc = None
        STATUS.unblock_error_polling()
        # clean up return method variable
        self._touchoff_return = None
        self._touchoff_error_return = None

    #------- boiler code

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


#############################

class Progress:
    def __init__(self, phases, total):
        self.num_phases = phases
        self.phase = 0
        self.total = total or 1
        self.lastcount = 0
        self.text = None

    def update(self, count, force=0):
        if force or count - self.lastcount > 400:
            fraction = (self.phase + count * 1. / self.total) / self.num_phases
            self.lastcount = count
            self.emit_percent(int(fraction * 100))

    # Send out progress
    def emit_percent(self, percent):
        STATUS.emit('progress', percent, self.text)

    def nextphase(self, total):
        self.phase += 1
        self.total = total or 1
        self.lastcount = -100
        self.update(0, True)

    def done(self):
        self.emit_percent(-1)

    # text for filter progress bar
    def set_text(self, text):
        if text is None:
            self.text = "Filter Progress"
        else:
            self.text = text


###########################################
# Filter Class
########################################################################
import os, sys, select, re
import tempfile, atexit, shutil

# slightly reworked code from gladevcp
# loads a filter program and collects the result
progress_re = re.compile("^FILTER_PROGRESS=(\\d*)$")


class FilterProgram:
    def __init__(self, program_filter, infilename, outfilename, callback=None):
        import subprocess
        outfile = open(outfilename, "w")
        infilename_q = infilename.replace("'", "'\\''")

        p = subprocess.Popen(["sh", "-c", "%s '%s'" % (program_filter, infilename_q)],
                             stdin=subprocess.PIPE,
                             stdout=outfile,
                             stderr=subprocess.PIPE)
        p.stdin.close()  # No input for you

        self.p = p
        self.stderr_text = []
        self.program_filter = os.path.split(program_filter)[1]
        self.filtered_program = os.path.split(infilename_q)[1]
        self.callback = callback

        self.progress = Progress(1, 100)
        self.progress.set_text("Filtering...")
        self.gid = STATUS.connect('periodic', self.update)

    def update(self, w):
        # check if done
        if self.p.poll() is not None:
            self.finish()
            STATUS.disconnect(self.gid)
            return False
        # check if there is something to read or pass
        r, w, x = select.select([self.p.stderr], [], [], 0)
        if not r:
            return True
        # process message from standard error
        stderr_line = self.p.stderr.readline()
        stderr_line = stderr_line.decode("utf-8")
        # compare to pre compiled re string
        # if true : update progress
        # else add it too error message string for later
        m = progress_re.match(stderr_line)
        if m:
            self.progress.update(int(m.group(1)), 1)
        else:
            self.stderr_text.append(stderr_line)
            sys.stderr.write(stderr_line)
        return True

    def finish(self):
        self.progress.done()
        # .. might be something left on stderr
        for line in self.p.stderr:
            line = line.decode("utf-8")
            m = progress_re.match(line)
            if not m:
                self.stderr_text.append(line)
        r = self.p.returncode
        if r:
            self.error(r, "".join(self.stderr_text))
        if self.callback:
            self.callback(r)

    # request an error dialog box
    def error(self, exitcode, stderr):
        message = '''The filter program '{}' that was filtering '{}'
                        exited with an error'''.format(self.program_filter, self.filtered_program)
        if stderr != '':
            more = "The error messages it produced are shown below:"
        else:
            more = None
        mess = {'NAME': 'MESSAGE', 'ID': 'ACTION_ERROR__',
                'MESSAGE': message,
                'MORE': more,
                'DETAILS': stderr,
                'ICON': 'CRITICAL',
                'FOCUS_TEXT': 'Filter program Error',
                'TITLE': 'Program Filter Error'}
        STATUS.emit('dialog-request', mess)
        LOG.error('Filter Program Error:{}'.format(stderr))


# For testing purposes

if __name__ == "__main__":
    from qtvcp.core import Action

    testcase = Action()


    # print status caught errors
    def mess(error, text):
        print('STATUS caught:', text)


    STATUS.connect("error", lambda w, n, d: mess(n, d))

    # test case
    testcase.SAVE_PROGRAM('hi', '/../../home')
