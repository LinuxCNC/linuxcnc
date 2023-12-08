#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import _hal, hal
import linuxcnc
import os
import math

from gi.repository import GObject
from gi.repository import GLib

# constants
JOGJOINT  = 1
JOGTELEOP = 0

# add try for QtVCP Designer and probably GTK GLADE editor too
# The INI file is not available then
try:
    inifile = linuxcnc.ini(os.environ['INI_FILE_NAME'])
    trajcoordinates = inifile.find("TRAJ", "COORDINATES").lower().replace(" ", "")
    jointcount = int(inifile.find("KINS", "JOINTS"))
except:
    pass
try:
    # get cycle time which could be in ms or seconds
    # convert to ms - use this to set update time
    ct = float(inifile.find('DISPLAY', 'CYCLE_TIME') or 100)
    if ct < 1:
        CYCLE_TIME = int(ct * 1000)
    else:
        CYCLE_TIME = int(ct)
except:
    CYCLE_TIME = 100

class GPin(GObject.Object, hal.Pin):
    __gtype_name__ = 'GPin'
    __gsignals__ = {'value-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ())}

    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        GObject.Object.__init__(self)
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
                print("Error updating pin %s; Removing" % p)
        for p in kill:
            self.REGISTRY.remove(p)
        return self.UPDATE

    @classmethod
    def update_start(self, timeout=100):
        if GPin.UPDATE:
            return
        GPin.UPDATE = True
        GLib.timeout_add(timeout, self.update_all)

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

class _GStat(GObject.GObject):
    '''Emits signals based on linuxcnc status '''
    __gsignals__ = {
        'periodic': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'state-estop': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'state-estop-reset': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'state-on': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'state-off': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),

        'homed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'unhomed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'all-homed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'not-all-homed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'override-limits-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN, GObject.TYPE_PYOBJECT,)),

        'hard-limits-tripped': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN, GObject.TYPE_PYOBJECT,)),

        'mode-manual': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'mode-auto': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'mode-mdi': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),

        'command-running': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'command-stopped': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'command-error': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),

        'interp-run': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'interp-idle': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'interp-paused': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'interp-reading': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'interp-waiting': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),

        'jograte-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'jograte-angular-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'jogincrement-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_FLOAT, GObject.TYPE_STRING)),
        'jogincrement-angular-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_FLOAT, GObject.TYPE_STRING)),

        'joint-selection-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'axis-selection-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),

        'program-pause-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'optional-stop-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'block-delete-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),

        'file-loaded': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'reload-display': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'line-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),

        'tool-in-spindle-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'tool-prep-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'tool-info-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
        'current-tool-offset': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),

        'motion-mode-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'motion-type-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'spindle-control-changed': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE,
             (GObject.TYPE_INT, GObject.TYPE_BOOLEAN, GObject.TYPE_INT, GObject.TYPE_BOOLEAN)),
        'current-feed-rate': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'current-x-rel-position': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'current-position': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,GObject.TYPE_PYOBJECT,
                            GObject.TYPE_PYOBJECT, GObject.TYPE_PYOBJECT,)),
        'current-z-rotation': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'requested-spindle-speed-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'actual-spindle-speed-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'spindle-override-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'feed-override-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'rapid-override-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'max-velocity-override-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),

        'feed-hold-enabled-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),

        'g90-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'g91-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'itime-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'fpm-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'fpr-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'css-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'rpm-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'radius-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'diameter-mode': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'flood-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'mist-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),

        'm-code-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'g-code-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'f-code-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        's-code-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT,)),
        'blend-code-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_FLOAT, GObject.TYPE_FLOAT)),

        'metric-mode-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
        'user-system-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),

        'mdi-line-selected': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING, GObject.TYPE_STRING)),
        'gcode-line-selected': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'graphics-line-selected': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'graphics-loading-progress': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'graphics-gcode-error': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'graphics-gcode-properties': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
        'graphics-view-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING, GObject.TYPE_PYOBJECT)),
        'mdi-history-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, ()),
        'machine-log-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, ()),
        'update-machine-log': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING, GObject.TYPE_STRING)),
        'move-text-lineup': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, ()),
        'move-text-linedown': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, ()),
        'dialog-request': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
        'focus-overlay-changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN, GObject.TYPE_STRING,
                            GObject.TYPE_PYOBJECT)),
        'play-sound': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'virtual-keyboard': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
        'dro-reference-change-request': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'system_notify_button_pressed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE,
                                        (GObject.TYPE_STRING, GObject.TYPE_BOOLEAN)),
        'show-preference': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'shutdown': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'error': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT, GObject.TYPE_STRING)),
        'general': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
        'forced-update': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, ()),
        'progress': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE, (GObject.TYPE_INT, GObject.TYPE_PYOBJECT)),
        'following-error': (GObject.SignalFlags.RUN_FIRST , GObject.TYPE_NONE,(GObject.TYPE_PYOBJECT,)),
        }

    STATES = { linuxcnc.STATE_ESTOP:       'state-estop'
             , linuxcnc.STATE_ESTOP_RESET: 'state-estop-reset'
             , linuxcnc.STATE_ON:          'state-on'
             , linuxcnc.STATE_OFF:         'state-off'
             }

    MODES  = { linuxcnc.MODE_MANUAL: 'mode-manual'
             , linuxcnc.MODE_AUTO:   'mode-auto'
             , linuxcnc.MODE_MDI:    'mode-mdi'
             }

    INTERP = { linuxcnc.INTERP_WAITING: 'interp-waiting'
             , linuxcnc.INTERP_READING: 'interp-reading'
             , linuxcnc.INTERP_PAUSED: 'interp-paused'
             , linuxcnc.INTERP_IDLE: 'interp-idle'
             }

    TEMPARARY_MESSAGE = 255
    OPERATOR_ERROR = linuxcnc.OPERATOR_ERROR
    OPERATOR_TEXT = linuxcnc.OPERATOR_TEXT
    NML_ERROR = linuxcnc.NML_ERROR
    NML_TEXT = linuxcnc.NML_TEXT

    MANUAL = linuxcnc.MODE_MANUAL
    AUTO = linuxcnc.MODE_AUTO
    MDI = linuxcnc.MODE_MDI

    STATE_ESTOP = linuxcnc.STATE_ESTOP
    STATE_ESTOP_RESET = linuxcnc.STATE_ESTOP_RESET
    STATE_ON = linuxcnc.STATE_ON
    STATE_OFF = linuxcnc.STATE_OFF

    def __init__(self, stat = None):
        GObject.Object.__init__(self)
        self.stat = stat or linuxcnc.stat()
        self.cmd = linuxcnc.command()
        self._status_active = False
        self.old = {}
        self.old['tool-prep-number'] = 0
        self.previous_mode = self.MANUAL
        try:
            self.stat.poll()
            self.merge()
        except:
            pass

        self.current_jog_rate = 15
        self.current_angular_jog_rate = 360
        self.current_jog_distance = 0
        self.current_jog_distance_text =''
        self.current_jog_distance_angular= 0
        self.current_jog_distance_angular_text =''
        self.selected_joint = -1
        self.selected_axis = ''
        self._is_all_homed = False
        self.set_timer()

    # we put this in a function so qtvcp
    # can override it to fix a seg fault
    def set_timer(self):
        GLib.timeout_add(CYCLE_TIME, self.update)

    def merge(self):
        self.old['command-state'] = self.stat.state
        self.old['state'] = self.stat.task_state
        self.old['mode']  = self.stat.task_mode
        self.old['interp']= self.stat.interp_state
        # Only update file if call level is 0, which
        # means we are not executing a subroutine/remap
        # This avoids emitting signals for bogus file names below
        if self.stat.call_level == 0:
            self.old['file']  = self.stat.file
        self.old['paused']= self.stat.paused
        self.old['line']  = self.stat.motion_line
        self.old['homed'] = self.stat.homed
        self.old['tool-in-spindle'] = self.stat.tool_in_spindle
        try:
            if hal.get_value('iocontrol.0.tool-prepare'):
                self.old['tool-prep-number'] = hal.get_value('iocontrol.0.tool-prep-number')
        except RuntimeError:
             self.old['tool-prep-number'] = -1
        self.old['motion-mode'] = self.stat.motion_mode
        self.old['motion-type'] = self.stat.motion_type
        self.old['spindle-or'] = self.stat.spindle[0]['override']
        self.old['feed-or'] = self.stat.feedrate
        self.old['rapid-or'] = self.stat.rapidrate
        self.old['max-velocity-or'] = self.stat.max_velocity
        self.old['feed-hold']  = self.stat.feed_hold_enabled
        self.old['g5x-index']  = self.stat.g5x_index
        self.old['spindle-enabled']  = self.stat.spindle[0]['enabled']
        self.old['spindle-direction']  = self.stat.spindle[0]['direction']
        self.old['block-delete']= self.stat.block_delete
        self.old['optional-stop']= self.stat.optional_stop
        try:
            self.old['actual-spindle-speed'] = hal.get_value('spindle.0.speed-in') * 60
        except RuntimeError:
             self.old['actual-spindle-speed'] = 0
        try:
            self.old['spindle-at-speed'] = hal.get_value('spindle.0.at-speed')
        except RuntimeError:
            self.old['spindle-at-speed'] = False
        self.old['flood']= self.stat.flood
        self.old['mist']= self.stat.mist
        self.old['current-z-rotation'] = self.stat.rotation_xy
        self.old['current-tool-offset'] = self.stat.tool_offset

        # override limits / hard limits
        or_limit_list=[]
        hard_limit_list = []
        ferror = []
        hard_limit = False
        or_limit_set = False
        for j in range(0, self.stat.joints):
            or_limit_list.append( self.stat.joint[j]['override_limits'])
            or_limit_set = or_limit_set or self.stat.joint[j]['override_limits']
            min_hard_limit = self.stat.joint[j]['min_hard_limit']
            max_hard_limit = self.stat.joint[j]['max_hard_limit']
            hard_limit = hard_limit or min_hard_limit or max_hard_limit
            hard_limit_list.append([min_hard_limit,max_hard_limit])
            ferror.append(self.stat.joint[j]['ferror_current'])
        self.old['override-limits'] = or_limit_list
        self.old['override-limits-set'] = bool(or_limit_set)
        self.old['hard-limits-tripped'] = bool(hard_limit)
        self.old['hard-limits-list'] = hard_limit_list
        self.old['ferror-current'] = ferror

        # active G-codes
        active_gcodes = []
        codes =''
        for i in sorted(self.stat.gcodes[1:]):
            if i == -1: continue
            if i % 10 == 0:
                    active_gcodes.append("G%d" % (i/10))
            else:
                    active_gcodes.append("G%d.%d" % (i/10, i%10))
        for i in active_gcodes:
            codes = codes +('%s '%i)
        self.old['g-code'] = codes
        # extract specific G-code modes
        itime = fpm = fpr = css = rpm = metric = False
        radius = diameter = adm = idm = False
        for num,i in enumerate(active_gcodes):
            if i == 'G90': adm = True
            elif i == 'G91': idm = True
            elif i == 'G93': itime = True
            elif i == 'G94': fpm = True
            elif i == 'G95': fpr = True
            elif i == 'G96': css = True
            elif i == 'G97': rpm = True
            elif i == 'G21': metric = True
            elif i == 'G7': diameter  = True
            elif i == 'G8': radius = True
        self.old['g90'] = adm
        self.old['g91'] = idm
        self.old['itime'] = itime
        self.old['fpm'] = fpm
        self.old['fpr'] = fpr
        self.old['css'] = css
        self.old['rpm'] = rpm
        self.old['metric'] = metric
        self.old['radius'] = radius
        self.old['diameter'] = diameter
        if css:
            try:
                self.old['spindle-speed']= hal.get_value('spindle.0.speed-out')
            except RuntimeError:
                self.old['spindle-speed']= self.stat.spindle[0]['speed']
        else:
            self.old['spindle-speed']= self.stat.spindle[0]['speed']

        # active M-codes
        active_mcodes = []
        mcodes = ''
        for i in sorted(self.stat.mcodes[1:]):
            if i == -1: continue
            active_mcodes.append("M%d"%i )
        for i in active_mcodes:
            mcodes = mcodes + ("%s "%i)
            #active_mcodes.append("M%s "%i)
        self.old['m-code'] = mcodes
        self.old['tool-info']  = self.stat.tool_table[0]
        settings = self.stat.settings
        self.old['f-code'] = settings[1]
        self.old['s-code'] = settings[2]
        self.old['blend-tolerance-code'] = settings[3]
        self.old['nativecam-tolerance-code'] = settings[4]

    def update(self):
        try:
            self.stat.poll()
        except:
            self._status_active = False
            # some things might not need linuxcnc status but do need periodic
            self.emit('periodic')
            # Reschedule
            return True
        self._status_active = True
        old = dict(self.old)
        self.merge()
        cmd_state_old = old.get('command-state')
        cmd_state_new = self.old['command-state']
        if cmd_state_new != cmd_state_old:
            if cmd_state_new == linuxcnc.RCS_EXEC:
                self.emit('command-running')
            elif cmd_state_new == linuxcnc.RCS_DONE:
                self.emit('command-stopped')
            elif cmd_state_new == linuxcnc.RCS_ERROR:
                self.emit('command-error')

        state_old = old.get('state', 0)
        state_new = self.old['state']
        if state_new != state_old:

            # set machine estop/clear
            if state_old == linuxcnc.STATE_ESTOP and state_new == linuxcnc.STATE_ESTOP_RESET:
                self.emit('state-estop-reset')
                self.emit('interp-idle')
            elif state_new == linuxcnc.STATE_ESTOP:
                self.emit('state-estop')
                self.emit('interp-idle')

            # set machine on/off
            if state_new == linuxcnc.STATE_ON:
                self.emit('state-on')
            elif state_old == linuxcnc.STATE_ON and state_new < linuxcnc.STATE_ON:
                self.emit('state-off')

            # reset modes/interpreter on machine on
            if state_new == linuxcnc.STATE_ON:
                old['mode'] = 0
                old['interp'] = 0

        mode_old = old.get('mode', 0)
        mode_new = self.old['mode']
        if mode_new != mode_old:
            self.previous_mode = mode_old
            self.emit(self.MODES[mode_new])

        interp_old = old.get('interp', 0)
        interp_new = self.old['interp']
        if interp_new != interp_old:
            if not interp_old or interp_old == linuxcnc.INTERP_IDLE:
                self.emit('interp-run')
            self.emit(self.INTERP[interp_new])
        # paused
        paused_old = old.get('paused', None)
        paused_new = self.old['paused']
        if paused_new != paused_old:
            self.emit('program-pause-changed',paused_new)
        # block delete
        block_delete_old = old.get('block-delete', None)
        block_delete_new = self.old['block-delete']
        if block_delete_new != block_delete_old:
            self.emit('block-delete-changed',block_delete_new)
        # optional_stop
        optional_stop_old = old.get('optional-stop', None)
        optional_stop_new = self.old['optional-stop']
        if optional_stop_new != optional_stop_old:
            self.emit('optional-stop-changed',optional_stop_new)
        # file changed
        file_old = old.get('file', None)
        file_new = self.old['file']
        if file_new != file_old:
            # if interpreter is reading or waiting, the new file
            # is a remap procedure, with the following test we
            # partly avoid emitting a signal in that case, which would cause
            # a reload of the preview and sourceview widgets.  A signal could
            # still be emitted if aborting a program shortly after it ran an
            # external file subroutine, but that is fixed by not updating the
            # file name if call level != 0 in the merge() function above.
            # do avoid that a signal is emitted in that case, causing
            # a reload of the preview and sourceview widgets
            if self.stat.interp_state == linuxcnc.INTERP_IDLE:
                self.emit('file-loaded', file_new)

        #ToDo : Find a way to avoid signal when the line changed due to
        #       a remap procedure, because the signal do highlight a wrong
        #       line in the code
        # current line
        line_old = old.get('line', None)
        line_new = self.old['line']
        if line_new != line_old:
            self.emit('line-changed', line_new)

        tool_old = old.get('tool-in-spindle', None)
        tool_new = self.old['tool-in-spindle']
        if tool_new != tool_old:
            self.emit('tool-in-spindle-changed', tool_new)
        tool_num_old = old.get('tool-prep-number')
        tool_num_new = self.old['tool-prep-number']
        if tool_num_new != tool_num_old:
            self.emit('tool-prep-changed', tool_num_new)

        motion_mode_old = old.get('motion-mode', None)
        motion_mode_new = self.old['motion-mode']
        if motion_mode_new != motion_mode_old:
            self.emit('motion-mode-changed', motion_mode_new)

        motion_type_old = old.get('motion-type', None)
        motion_type_new = self.old['motion-type']
        if motion_type_new != motion_type_old:
            self.emit('motion-type-changed', motion_type_new)

        # if the homed status has changed
        # check number of homed joints against number of available joints
        # if they are equal send the all-homed signal
        # else send the not-all-homed signal (with a string of unhomed joint numbers)
        # if a joint is homed send 'homed' (with a string of homed joint number)
        homed_joint_old = old.get('homed', None)
        homed_joint_new = self.old['homed']
        if homed_joint_new != homed_joint_old:
            homed_joints = 0
            unhomed_joints = ""
            for joint in range(0, self.stat.joints):
                if self.stat.homed[joint]:
                    homed_joints += 1
                    self.emit('homed', joint)
                else:
                    self.emit('unhomed', joint)
                    unhomed_joints += str(joint)
            if homed_joints == self.stat.joints:
                self._is_all_homed = True
                self.emit('all-homed')
            else:
                self._is_all_homed = False
                self.emit('not-all-homed', unhomed_joints)

        # override limits
        or_limits_old = old.get('override-limits', None)
        or_limits_new = self.old['override-limits']
        or_limits_set_new = self.old['override-limits-set']
        if or_limits_new != or_limits_old:
            self.emit('override-limits-changed',or_limits_set_new, or_limits_new)
        # hard limits tripped
        t_list_old = old.get('hard-limits-list')
        t_list_new = self.old['hard-limits-list']
        if t_list_new != t_list_old:
            hard_limits_tripped_new = self.old['hard-limits-tripped']
            self.emit('hard-limits-tripped',hard_limits_tripped_new, t_list_new)
        # current velocity
        self.emit('current-feed-rate',self.stat.current_vel * 60.0)
        # X relative position
        position = self.stat.actual_position[0]
        g5x_offset = self.stat.g5x_offset[0]
        tool_offset = self.stat.tool_offset[0]
        g92_offset = self.stat.g92_offset[0]
        self.emit('current-x-rel-position',position-g5x_offset-tool_offset-g92_offset)

        # calculate position offsets (native units)
        p,rel_p,dtg = self.get_position()
        self.emit('current-position',p, rel_p, dtg, self.stat.joint_actual_position)

        # ferror
        self.emit('following-error', self.old['ferror-current'])

        # spindle control
        spindle_enabled_old = old.get('spindle-enabled', None)
        spindle_enabled_new = self.old['spindle-enabled']
        spindle_direction_old = old.get('spindle-direction', None)
        spindle_direction_new = self.old['spindle-direction']
        up_to_speed_old = old.get('spindle-at-speed',None)
        up_to_speed_new = self.old['spindle-at-speed']
        if up_to_speed_new != up_to_speed_old or \
            spindle_enabled_new != spindle_enabled_old or \
            spindle_direction_new != spindle_direction_old:
                self.emit('spindle-control-changed', 0, spindle_enabled_new, spindle_direction_new, up_to_speed_new)
        # requested spindle speed
        spindle_spd_old = old.get('spindle-speed', None)
        spindle_spd_new = self.old['spindle-speed']
        if spindle_spd_new != spindle_spd_old:
            self.emit('requested-spindle-speed-changed', spindle_spd_new)
        # actual spindle speed
        act_spindle_spd_old = old.get('actual-spindle-speed', None)
        act_spindle_spd_new = self.old['actual-spindle-speed']
        if act_spindle_spd_new != act_spindle_spd_old:
            self.emit('actual-spindle-speed-changed', act_spindle_spd_new)
        # spindle override
        spindle_or_old = old.get('spindle-or', None)
        spindle_or_new = self.old['spindle-or']
        if spindle_or_new != spindle_or_old:
            self.emit('spindle-override-changed',spindle_or_new * 100)
        # feed override
        feed_or_old = old.get('feed-or', None)
        feed_or_new = self.old['feed-or']
        if feed_or_new != feed_or_old:
            self.emit('feed-override-changed',feed_or_new * 100)
        # rapid override
        rapid_or_old = old.get('rapid-or', None)
        rapid_or_new = self.old['rapid-or']
        if rapid_or_new != rapid_or_old:
            self.emit('rapid-override-changed',rapid_or_new * 100)
        # max-velocity override
        max_velocity_or_old = old.get('max-velocity-or', None)
        max_velocity_or_new = self.old['max-velocity-or']
        if max_velocity_or_new != max_velocity_or_old:
            # work around misconfigured config (missing MAX_LINEAR_VELOCITY in TRAJ)
            if max_velocity_or_new != 1e99:
                self.emit('max-velocity-override-changed',max_velocity_or_new * 60)
        # feed hold
        feed_hold_old = old.get('feed-hold', None)
        feed_hold_new = self.old['feed-hold']
        if feed_hold_new != feed_hold_old:
            self.emit('feed-hold-enabled-changed',feed_hold_new)
        # mist
        mist_old = old.get('mist', None)
        mist_new = self.old['mist']
        if mist_new != mist_old:
            self.emit('mist-changed',mist_new)
        # flood
        flood_old = old.get('flood', None)
        flood_new = self.old['flood']
        if flood_new != flood_old:
            self.emit('flood-changed',flood_new)
        # rotation around Z
        z_rot_old = old.get('current-z-rotation', None)
        z_rot_new = self.old['current-z-rotation']
        if z_rot_new != z_rot_old:
            self.emit('current-z-rotation',z_rot_new)
        # current tool offsets
        tool_off_old = old.get('current-tool-offset', None)
        tool_off_new = self.old['current-tool-offset']
        if tool_off_new != tool_off_old:
               self.emit('current-tool-offset',tool_off_new)
        #############################
        # Gcodes
        #############################
        # G-codes
        g_code_old = old.get('g-code', None)
        g_code_new = self.old['g-code']
        if g_code_new != g_code_old:
            self.emit('g-code-changed',g_code_new)
        # metric mode g21
        metric_old = old.get('metric', None)
        metric_new = self.old['metric']
        if metric_new != metric_old:
            self.emit('metric-mode-changed',metric_new)
        # G5x (active user system)
        g5x_index_old = old.get('g5x-index', None)
        g5x_index_new = self.old['g5x-index']
        if g5x_index_new != g5x_index_old:
            self.emit('user-system-changed',g5x_index_new)

        # absolute mode g90
        g90_old = old.get('g90', None)
        g90_new = self.old['g90']
        if g90_new != g90_old:
            self.emit('g90-mode',g90_new)

        # incremental mode g91
        g91_old = old.get('g91', None)
        g91_new = self.old['g91']
        if g91_new != g91_old:
            self.emit('g91-mode',g91_new)

        # inverse time mode g93
        itime_old = old.get('itime', None)
        itime_new = self.old['itime']
        if itime_new != itime_old:
            self.emit('itime-mode',itime_new)
        # feed per minute mode g94
        fpm_old = old.get('fpm', None)
        fpm_new = self.old['fpm']
        if fpm_new != fpm_old:
            self.emit('fpm-mode',fpm_new)
        # feed per revolution mode g95
        fpr_old = old.get('fpr', None)
        fpr_new = self.old['fpr']
        if fpr_new != fpr_old:
            self.emit('fpr-mode',fpr_new)
        # css mode g96
        css_old = old.get('css', None)
        css_new = self.old['css']
        if css_new != css_old:
            self.emit('css-mode',css_new)
        # rpm mode g97
        rpm_old = old.get('rpm', None)
        rpm_new = self.old['rpm']
        if rpm_new != rpm_old:
            self.emit('rpm-mode',rpm_new)
        # radius mode g8
        radius_old = old.get('radius', None)
        radius_new = self.old['radius']
        if radius_new != radius_old:
            self.emit('radius-mode',radius_new)
        # diameter mode g7
        diam_old = old.get('diameter', None)
        diam_new = self.old['diameter']
        if diam_new != diam_old:
            self.emit('diameter-mode',diam_new)
        ####################################
        # Mcodes
        ####################################
        # M-codes
        m_code_old = old.get('m-code', None)
        m_code_new = self.old['m-code']
        if m_code_new != m_code_old:
            self.emit('m-code-changed',m_code_new)
        tool_info_old = old.get('tool-info', None)
        tool_info_new = self.old['tool-info']
        if tool_info_new != tool_info_old:
            self.emit('tool-info-changed', tool_info_new)

        #####################################
        # settings
        #####################################
        # feed code
        f_code_old = old.get('f-code', None)
        f_code_new = self.old['f-code']
        if f_code_new != f_code_old:
            self.emit('f-code-changed',f_code_new)

        # s code
        s_code_old = old.get('s-code', None)
        s_code_new = self.old['s-code']
        if s_code_new != s_code_old:
            self.emit('s-code-changed',s_code_new)

        # g53 blend code
        blend_code_old = old.get('blend-tolerance-code', None)
        blend_code_new = self.old['blend-tolerance-code']
        cam_code_old = old.get('nativecam-tolerance-code', None)
        cam_code_new = self.old['nativecam-tolerance-code']

        if blend_code_new != blend_code_old or \
           blend_code_new != blend_code_old:
                self.emit('blend-code-changed',blend_code_new, cam_code_new)

        # AND DONE... Return true to continue timeout
        self.emit('periodic')
        return True

    def forced_update(self):
        try:
            self.stat.poll()
        except:
            # Reschedule
            return True
        self.merge()
        cmd_state_new = self.old['command-state']
        if cmd_state_new == linuxcnc.RCS_EXEC:
            self.emit('command-running')
        elif cmd_state_new == linuxcnc.RCS_DONE:
            self.emit('command-stopped')
        elif cmd_state_new == linuxcnc.RCS_ERROR:
            self.emit('command-error')

        state_new = self.old['state']
        if state_new > linuxcnc.STATE_ESTOP:
            self.emit('state-estop-reset')
        else:
            self.emit('state-estop')
        self.emit('state-off')
        self.emit('interp-idle')
        # override limits
        or_limits_new = self.old['override-limits']
        or_limits_set_new = self.old['override-limits-set']
        self.emit('override-limits-changed',or_limits_set_new, or_limits_new)
        # hard limits tripped
        t_list_new = self.old['hard-limits-list']
        hard_limits_tripped_new = self.old['hard-limits-tripped']
        self.emit('hard-limits-tripped',hard_limits_tripped_new, t_list_new)
        # overrides
        feed_or_new = self.old['feed-or']
        self.emit('feed-override-changed',feed_or_new * 100)
        rapid_or_new = self.old['rapid-or']
        self.emit('rapid-override-changed',rapid_or_new  * 100)
        max_velocity_or_new = self.old['max-velocity-or']
        # work around misconfigured config (missing MAX_LINEAR_VELOCITY in TRAJ)
        if max_velocity_or_new != 1e99:
            self.emit('max-velocity-override-changed',max_velocity_or_new * 60)
        spindle_or_new = self.old['spindle-or']
        self.emit('spindle-override-changed',spindle_or_new  * 100)

        # spindle speed mpde
        css_new = self.old['css']
        self.emit('css-mode',css_new)
        rpm_new = self.old['rpm']
        self.emit('rpm-mode',rpm_new)

        # absolute mode g90
        g90_new = self.old['g90']
        self.emit('g90-mode',g90_new)
        # incremental mode g91
        g91_new = self.old['g91']
        self.emit('g91-mode',g91_new)
        # feed mode:
        itime_new = self.old['itime']
        self.emit('itime-mode',itime_new)
        fpm_new = self.old['fpm']
        self.emit('fpm-mode',fpm_new)
        fpr_new = self.old['fpr']
        self.emit('fpr-mode',fpr_new)
        # paused
        paused_new = self.old['paused']
        self.emit('program-pause-changed',paused_new)
        # block delete
        block_delete_new = self.old['block-delete']
        self.emit('block-delete-changed',block_delete_new)
        # optional_stop
        optional_stop_new = self.old['optional-stop']
        self.emit('optional-stop-changed',optional_stop_new)
        # user system G5x
        system_new = self.old['g5x-index']
        self.emit('user-system-changed',system_new)
        # radius mode g8
        radius_new = self.old['radius']
        self.emit('radius-mode',radius_new)
        # diameter mode g7
        diam_new = self.old['diameter']
        self.emit('diameter-mode',diam_new)
        # rotation around Z
        z_rot_new = self.old['current-z-rotation']
        self.emit('current-z-rotation',z_rot_new)
        # current tool offsets
        tool_off_new = self.old['current-tool-offset']
        self.emit('current-tool-offset',tool_off_new)

        # M-codes
        m_code_new = self.old['m-code']
        self.emit('m-code-changed',m_code_new)
        flood_new = self.old['flood']
        self.emit('flood-changed',flood_new)
        mist_new = self.old['mist']
        self.emit('mist-changed',mist_new)

        # G-codes
        g_code_new = self.old['g-code']
        self.emit('g-code-changed',g_code_new)
        # metric units G21
        metric_new = self.old['metric']
        self.emit('metric_mode_changed',metric_new)
        # tool in spindle
        tool_new = self.old['tool-in-spindle']
        self.emit('tool-in-spindle-changed', tool_new)
        tool_num_new = self.old['tool-prep-number']
        self.emit('tool-prep-changed', tool_num_new)

        # feed code
        f_code_new = self.old['f-code']
        self.emit('f-code-changed',f_code_new)

        # s code
        s_code_new = self.old['s-code']
        self.emit('s-code-changed',s_code_new)

        # g53 blend code
        blend_code_new = self.old['blend-tolerance-code']
        cam_code_new = self.old['nativecam-tolerance-code']
        self.emit('blend-code-changed',blend_code_new, cam_code_new)

        # Trajectory Motion mode
        motion_mode_new = self.old['motion-mode']
        self.emit('motion-mode-changed', motion_mode_new)

        # Trajectory Motion type
        motion_type_new = self.old['motion-type']
        self.emit('motion-type-changed', motion_type_new)

        # Spindle requested speed
        spindle_spd_new = self.old['spindle-speed']
        self.emit('requested-spindle-speed-changed', spindle_spd_new)
        spindle_spd_new = self.old['actual-spindle-speed']
        self.emit('actual-spindle-speed-changed', spindle_spd_new)
        self.emit('spindle-control-changed', 0, False, 0, False)
        self.emit('jograte-changed', self.current_jog_rate)
        self.emit('jograte-angular-changed', self.current_angular_jog_rate)
        self.emit('jogincrement-changed', self.current_jog_distance, self.current_jog_distance_text)
        self.emit('jogincrement-angular-changed', self.current_jog_distance_angular, self.current_jog_distance_angular_text)
        tool_info_new = self.old['tool-info']
        self.emit('tool-info-changed', tool_info_new)

        # homing
        homed_joints = 0
        unhomed_joints = ""
        for joint in range(0, self.stat.joints):
            if self.stat.homed[joint]:
                homed_joints += 1
                self.emit('homed', joint)
            else:
                self.emit('unhomed', joint)
                unhomed_joints += str(joint)
        if homed_joints == self.stat.joints:
            self._is_all_homed = True
            self.emit('all-homed')
        else:
            self._is_all_homed = False
            self.emit('not-all-homed', unhomed_joints)

        # ferror
        self.emit('following-error', self.old['ferror-current'])

        # update external objects
        self.emit('forced-update')

    # ********** Helper function ********************
    def get_position(self):
        p = self.stat.actual_position
        mp = self.stat.position
        dtg = self.stat.dtg

        x = p[0] - self.stat.g5x_offset[0] - self.stat.tool_offset[0]
        y = p[1] - self.stat.g5x_offset[1] - self.stat.tool_offset[1]
        z = p[2] - self.stat.g5x_offset[2] - self.stat.tool_offset[2]
        a = p[3] - self.stat.g5x_offset[3] - self.stat.tool_offset[3]
        b = p[4] - self.stat.g5x_offset[4] - self.stat.tool_offset[4]
        c = p[5] - self.stat.g5x_offset[5] - self.stat.tool_offset[5]
        u = p[6] - self.stat.g5x_offset[6] - self.stat.tool_offset[6]
        v = p[7] - self.stat.g5x_offset[7] - self.stat.tool_offset[7]
        w = p[8] - self.stat.g5x_offset[8] - self.stat.tool_offset[8]

        if self.stat.rotation_xy != 0:
            t = math.radians(-self.stat.rotation_xy)
            xr = x * math.cos(t) - y * math.sin(t)
            yr = x * math.sin(t) + y * math.cos(t)
            x = xr
            y = yr

        x -= self.stat.g92_offset[0]
        y -= self.stat.g92_offset[1]
        z -= self.stat.g92_offset[2]
        a -= self.stat.g92_offset[3]
        b -= self.stat.g92_offset[4]
        c -= self.stat.g92_offset[5]
        u -= self.stat.g92_offset[6]
        v -= self.stat.g92_offset[7]
        w -= self.stat.g92_offset[8]

        relp = [x, y, z, a, b, c, u, v, w]
        return p,relp,dtg

    # check for required modes
    # fail if mode is 0
    # fail if machine is busy
    # true if all ready in mode
    # None if possible to change
    def check_for_modes(self, *modes):
        def running(s):
            return s.task_mode == linuxcnc.MODE_AUTO and s.interp_state != linuxcnc.INTERP_IDLE
        self.stat.poll()
        premode = self.stat.task_mode
        if not modes: return (None, premode)
        try:
            if  self.stat.task_mode in modes[0]: return (True, premode)
        except:
            if  self.stat.task_mode == modes[0]: return (True, premode)
        if running( self.stat): return (None, premode)
        return (False, premode)

    def get_current_mode(self):
        return self.old['mode']

    def get_previous_mode(self):
        return self.previous_mode

    # linear - in machine units
    def set_jograte(self, upm):
        self.current_jog_rate = upm
        self.emit('jograte-changed', upm)

    def get_jograte(self):
        return self.current_jog_rate

    def set_jograte_angular(self,rate):
        self.current_angular_jog_rate = rate
        self.emit('jograte-angular-changed', rate)

    def get_jograte_angular(self):
        return self.current_angular_jog_rate

    def get_jog_increment_angular(self):
        return self.current_jog_distance_angular

    def set_jog_increment_angular(self, distance, text):
        try:
            isinstance(float(distance), float)
        except:
            print('error converting angular jog increment ({}) to float: staying at {}'.format(distance,self.current_jog_distance_angular))
            return
        self.current_jog_distance_angular = distance
        self.current_jog_distance_text_angular = text
        self.emit('jogincrement-angular-changed', distance, text)

    # should be in machine units
    def set_jog_increments(self, distance, text):
        try:
            isinstance(float(distance), float)
        except:
            print('error converting jog increment ({}) to float: staying at {}'.format(distance,self.current_jog_distance))
            return
        self.current_jog_distance = distance
        self.current_jog_distance_text = text
        self.emit('jogincrement-changed', distance, text)

    def get_jog_increment(self):
        return self.current_jog_distance

    def get_max_velocity(self):
        return self.old['max-velocity-or'] * 60

    def set_selected_joint(self, data):
        self.selected_joint = int(data)
        self.emit('joint-selection-changed', data)

    def get_selected_joint(self):
        return self.selected_joint

    def set_selected_axis(self, data):
        self.selected_axis = str(data)
        self.emit('axis-selection-changed', data)

    def get_selected_axis(self):
        return self.selected_axis

    def is_joint_homed(self, joint):
        self.stat.poll()
        return bool(self.stat.homed[joint])

    def is_all_homed(self):
        return self._is_all_homed

    def is_homing(self):
        for j in range(0, self.stat.joints):
            if self.stat.joint[j].get('homing'):
                return True
        return False

    def machine_is_on(self):
        return self.old['state']  > linuxcnc.STATE_OFF

    def estop_is_clear(self):
        self.stat.poll()
        return self.stat.task_state > linuxcnc.STATE_ESTOP

    def is_man_mode(self):
        self.stat.poll()
        return self.stat.task_mode  == linuxcnc.MODE_MANUAL

    def is_mdi_mode(self):
        self.stat.poll()
        return self.stat.task_mode  == linuxcnc.MODE_MDI

    def is_auto_mode(self):
        self.stat.poll()
        return self.stat.task_mode  == linuxcnc.MODE_AUTO

    def is_on_and_idle(self):
        self.stat.poll()
        return self.stat.task_state > linuxcnc.STATE_OFF and self.stat.interp_state == linuxcnc.INTERP_IDLE

    def is_auto_running(self):
        self.stat.poll()
        return self.stat.task_mode == linuxcnc.MODE_AUTO and self.stat.interp_state != linuxcnc.INTERP_IDLE

    def is_auto_paused(self):
        return self.old['paused']

    def is_interp_running(self):
        self.stat.poll()
        return self.stat.interp_state != linuxcnc.INTERP_IDLE

    def is_interp_paused(self):
        self.stat.poll()
        return self.stat.interp_state == linuxcnc.INTERP_PAUSED

    def is_interp_reading(self):
        self.stat.poll()
        return self.stat.interp_state == linuxcnc.INTERP_READING

    def is_interp_waiting(self):
        self.stat.poll()
        return self.stat.interp_state == linuxcnc.INTERP_WAITING

    def is_interp_idle(self):
        self.stat.poll()
        return self.stat.interp_state == linuxcnc.INTERP_IDLE

    def is_file_loaded(self):
        self.stat.poll()
        if self.stat.file:
            return True
        else:
            return False

    def is_metric_mode(self):
        return self.old['metric']

    def is_spindle_on(self, num = 0):
        self.stat.poll()
        return self.stat.spindle[num]['enabled']

    def get_spindle_speed(self, num=0):
        self.stat.poll()
        return self.stat.spindle[num]['speed']

    def is_joint_mode(self):
        try:
            self.stat.poll()
        except:
            return None
        return bool(self.stat.motion_mode == linuxcnc.TRAJ_MODE_FREE)

    def is_world_mode(self):
        try:
            self.stat.poll()
        except:
            return None
        return bool(self.stat.motion_mode == linuxcnc.TRAJ_MODE_TELEOP)

    def is_status_valid(self):
        return self._status_active

    def is_limits_override_set(self):
        return self.old['override-limits-set']

    def is_hard_limits_tripped(self):
        return self.old['hard-limits-tripped']

    def get_current_tool(self):
        self.stat.poll()
        return self.stat.tool_in_spindle

    def set_tool_touchoff(self,tool,axis,value):
        premode = None
        m = "G10 L10 P%d %s%f"%(tool,axis,value)
        self.stat.poll()
        if self.stat.task_mode != linuxcnc.MODE_MDI:
            premode = self.stat.task_mode
            self.cmd.mode(linuxcnc.MODE_MDI)
            self.cmd.wait_complete()
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        self.cmd.mdi("g43")
        self.cmd.wait_complete()
        if premode:
            self.cmd.mode(premode)

    def set_axis_origin(self,axis,value):
        premode = None
        m = "G10 L20 P0 %s%f"%(axis,value)
        self.stat.poll()
        if self.stat.task_mode != linuxcnc.MODE_MDI:
            premode = self.stat.task_mode
            self.cmd.mode(linuxcnc.MODE_MDI)
            self.cmd.wait_complete()
        self.cmd.mdi(m)
        self.cmd.wait_complete()
        if premode:
            self.cmd.mode(premode)
        self.emit('reload-display')

    def do_jog(self, axisnum, direction, distance=0):
        jjogmode,j_or_a = self.get_jog_info(axisnum)
        if direction == 0:
            self.cmd.jog(linuxcnc.JOG_STOP, jjogmode, j_or_a)
        else:
            if axisnum in (3,4,5):
                rate = self.current_angular_jog_rate
            else:
                rate = self.current_jog_rate/60
            if distance == 0:
                self.cmd.jog(linuxcnc.JOG_CONTINUOUS, jjogmode, j_or_a, direction * rate)
            else:
                self.cmd.jog(linuxcnc.JOG_INCREMENT, jjogmode, j_or_a, direction * rate, distance)

    def get_jjogmode(self):
        self.stat.poll()
        if self.stat.motion_mode == linuxcnc.TRAJ_MODE_FREE:
            return JOGJOINT
        if self.stat.motion_mode == linuxcnc.TRAJ_MODE_TELEOP:
            return JOGTELEOP
        print("commands.py: unexpected motion_mode",self.stat.motion_mode)
        return JOGTELEOP

    def jnum_for_axisnum(self,axisnum):
        if self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            print("\n%s:\n  Joint jogging not supported for"
                   "non-identity kinematics"%__file__)
            return -1 # emcJogCont() et al reject neg joint/axis no.s
        jnum = trajcoordinates.index( "xyzabcuvw"[axisnum] )
        if jnum > jointcount:
            print("\n%s:\n  Computed joint number=%d for axisnum=%d "
                   "exceeds jointcount=%d with trajcoordinates=%s"
                   %(__file__,jnum,axisnum,jointcount,trajcoordinates))
            # Note: primary gui should protect for this misconfiguration
            # decline to jog
            return -1 # emcJogCont() et al reject neg joint/axis no.s
        return jnum

    def get_jog_info (self,axisnum):
        jjogmode = self.get_jjogmode()
        j_or_a = axisnum
        if jjogmode == JOGJOINT: j_or_a = self.jnum_for_axisnum(axisnum)
        return jjogmode,j_or_a

    def get_probed_position(self):
        self.stat.poll()
        return list(self.stat.probed_position)

    def get_probed_position_with_offsets(self) :
        self.stat.poll()
        probed_position=list(self.stat.probed_position)
        coord=list(self.stat.probed_position)
        g5x_offset=list(self.stat.g5x_offset)
        g92_offset=list(self.stat.g92_offset)
        tool_offset=list(self.stat.tool_offset)
        for i in range(0, len(probed_position)-1):
             coord[i] = probed_position[i] - g5x_offset[i] - g92_offset[i] - tool_offset[i]
        angl=self.stat.rotation_xy
        res=self._rott00_point(coord[0],coord[1],-angl)
        coord[0]=res[0]
        coord[1]=res[1]
        return coord

    # rotate around 0,0 point coordinates
    def _rott00_point(self,x1=0.,y1=0.,a1=0.) :
        coord = [x1,y1]
        if a1 != 0:
            t = math.radians(a1)
            coord[0] = x1 * math.cos(t) - y1 * math.sin(t)
            coord[1] = x1 * math.sin(t) + y1 * math.cos(t)
        return coord

    def shutdown(self):
        self.emit('shutdown')

    def get_linuxcnc_version(self):
        return linuxcnc.version

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
