#!/usr/bin/env python
# vim: sts=4 sw=4 et

import _hal, hal
from PyQt4.QtCore import QObject, QTimer, pyqtSignal
import linuxcnc
import math
import gobject

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


class _GStat(gobject.GObject):
    '''Emits signals based on linuxcnc status '''
    __gsignals__ = {
        'state-estop': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'state-estop-reset': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'state-on': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'state-off': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),

        'homed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'all-homed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'not-all-homed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'override_limits_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),

        'mode-manual': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'mode-auto': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'mode-mdi': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),

        'interp-run': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-idle': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-paused': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-reading': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-waiting': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),

        'jograte-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),

        'program-pause-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'optional-stop-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'block-delete-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),

        'file-loaded': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'reload-display': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'line-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_INT,)),

        'tool-in-spindle-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_INT,)),
        'spindle-control_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,gobject.TYPE_INT)),
        'current-feed-rate': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),
        'current-x-rel-position': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),
        'current-position': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,gobject.TYPE_PYOBJECT,
                            gobject.TYPE_PYOBJECT,)),
        'requested-spindle-speed-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),

        'spindle-override-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),
        'feed-override-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),
        'rapid-override-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),

        'feed-hold-enabled-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),

        'itime-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'fpm-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'fpr-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'css-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'rpm-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'radius-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'diameter-mode': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),

        'm-code-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'g-code-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),

        'metric-mode-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
        'user_system-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),

        'error-message': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'text-messsage': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'display-message': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
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

    def __init__(self, stat = None):
        gobject.GObject.__init__(self)
        self.stat = stat or linuxcnc.stat()
        self.error = linuxcnc.error_channel()
        self.cmd = linuxcnc.command()
        self.old = {}
        try:
            self.stat.poll()
            self.merge()
        except:
            pass
        gobject.timeout_add(100, self.update)
        self.current_jog_rate = 15
        self._is_all_homed = False

    def merge(self):
        self.old['state'] = self.stat.task_state
        self.old['mode']  = self.stat.task_mode
        self.old['interp']= self.stat.interp_state
        self.old['paused']= self.stat.paused
        self.old['file']  = self.stat.file
        self.old['line']  = self.stat.motion_line
        self.old['homed'] = self.stat.homed
        self.old['tool-in-spindle'] = self.stat.tool_in_spindle
        self.old['spindle-or'] = self.stat.spindlerate
        self.old['feed-or'] = self.stat.feedrate
        self.old['rapid-or'] = self.stat.rapidrate
        self.old['feed-hold']  = self.stat.feed_hold_enabled
        self.old['g5x-index']  = self.stat.g5x_index
        self.old['spindle-enabled']  = self.stat.spindle_enabled
        self.old['spindle-direction']  = self.stat.spindle_direction
        self.old['block-delete']= self.stat.block_delete
        self.old['optional-stop']= self.stat.optional_stop
        self.old['spindle-speed']= self.stat.spindle_speed

        # override limits
        or_limit_list=[]
        for i in range(0,8):
            or_limit_list.append( self.stat.axis[i]['override_limits'])
        self.old['override-limits'] = or_limit_list
        # active G codes
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
        # extract specific G code modes
        itime = fpm = fpr = css = rpm = metric = False
        radius = diameter = False
        for num,i in enumerate(active_gcodes):
            if i == 'G93': itime = True
            elif i == 'G94': fpm = True
            elif i == 'G95': fpr = True
            elif i == 'G96': css = True
            elif i == 'G97': rpm = True
            elif i == 'G21': metric = True
            elif i == 'G7': diameter  = True
            elif i == 'G8': radius = True
        self.old['itime'] = itime
        self.old['fpm'] = fpm
        self.old['fpr'] = fpr
        self.old['css'] = css
        self.old['rpm'] = rpm
        self.old['metric'] = metric
        self.old['radius'] = radius
        self.old['diameter'] = diameter

        # active M codes
        active_mcodes = ''
        for i in sorted(self.stat.mcodes[1:]):
            if i == -1: continue
            active_mcodes = active_mcodes + ("M%s "%i)
            #active_mcodes.append("M%s "%i)
        self.old['m-code'] = active_mcodes

    def update(self):
        try:
            e = self.error.poll()
            if e:
                kind, text = e
                if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                    self.emit('error-message',text)
                elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
                    self.emit('text-message',text)
                elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
                    self.emit('display-message',text)
        except:
            pass
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
                self.emit('state-estop-reset')
            else:
                self.emit('state-estop')
            self.emit('state-off')
            self.emit('interp-idle')

        if state_new != state_old:
            if state_old == linuxcnc.STATE_ON and state_new < linuxcnc.STATE_ON:
                self.emit('state-off')
            self.emit(self.STATES[state_new])
            if state_new == linuxcnc.STATE_ON:
                old['mode'] = 0
                old['interp'] = 0

        mode_old = old.get('mode', 0)
        mode_new = self.old['mode']
        if mode_new != mode_old:
            self.emit(self.MODES[mode_new])

        interp_old = old.get('interp', 0)
        interp_new = self.old['interp']
        if interp_new != interp_old:
            if not interp_old or interp_old == linuxcnc.INTERP_IDLE:
                print "Emit", "interp-run"
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
        optional_stop_old = old.get('optionaL-stop', None)
        optional_stop_new = self.old['optional-stop']
        if optional_stop_new != optional_stop_old:
            self.emit('optional-stop-changed',optional_stop_new)
        # file changed
        file_old = old.get('file', None)
        file_new = self.old['file']
        if file_new != file_old:
            # if interpreter is reading or waiting, the new file
            # is a remap procedure, with the following test we
            # do avoid that a signal is emited in that case, causing 
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

        # if the homed status has changed
        # check number of homed axes against number of available axes
        # if they are equal send the all-homed signal
        # else not-all-homed (with a string of unhomed joint numbers)
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
                self.emit('homed',homed)
            if count == axis_count:
                self.emit('all-homed')
                self._is_all_homed = True
            else:
                self.emit('not-all-homed',unhomed)
                self._is_all_homed = False
        # override limts
        or_limits_old = old.get('override-limits', None)
        or_limits_new = self.old['override-limits']
        if or_limits_new != or_limits_old:
            self.emit('override-limits-changed',or_limits_new)
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
        self.emit('current_position',p, rel_p, dtg)

        # spindle control
        spindle_enabled_old = old.get('spindle-enabled', None)
        spindle_enabled_new = self.old['spindle-enabled']
        spindle_direction_old = old.get('spindle-direction', None)
        spindle_direction_new = self.old['spindle-direction']
        if spindle_enabled_new != spindle_enabled_old or spindle_direction_new != spindle_direction_old:
            self.emit('spindle-control-changed', spindle_enabled_new, spindle_direction_new)
        # requested spindle speed
        spindle_spd_old = old.get('spindle-speed', None)
        spindle_spd_new = self.old['spindle-speed']
        if spindle_spd_new != spindle_spd_old:
            self.emit('requested-spindle-speed-changed', spindle_spd_new)
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
        # feed hold
        feed_hold_old = old.get('feed-hold', None)
        feed_hold_new = self.old['feed-hold']
        if feed_hold_new != feed_hold_old:
            self.emit('feed-hold-enabled-changed',feed_hold_new)
        # G5x (active user system)
        g5x_index_old = old.get('g5x-index', None)
        g5x_index_new = self.old['g5x-index']
        if g5x_index_new != g5x_index_old:
            self.emit('user-system-changed',g5x_index_new)
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
        # M codes
        m_code_old = old.get('m-code', None)
        m_code_new = self.old['m-code']
        if m_code_new != m_code_old:
            self.emit('m-code-changed',m_code_new)
        # G codes
        g_code_old = old.get('g-code', None)
        g_code_new = self.old['g-code']
        if g_code_new != g_code_old:
            self.emit('g-code-changed',g_code_new)
        # metric mode g21
        metric_old = old.get('metric', None)
        metric_new = self.old['metric']
        if metric_new != metric_old:
            self.emit('metric-mode-changed',metric_new)
        # AND DONE... Return true to continue timeout
        return True

    def forced_update(self):
        print 'Gstat forced update!'
        try:
            self.stat.poll()
        except:
            # Reschedule
            return True
        self.merge()

        # override limts
        or_limits_new = self.old['override-limits']
        self.emit('override-limits-changed',or_limits_new)
        # overrides
        feed_or_new = self.old['feed-or']
        self.emit('feed-override-changed',feed_or_new * 100)
        rapid_or_new = self.old['rapid-or']
        self.emit('rapid-override-changed',rapid_or_new  * 100)
        spindle_or_new = self.old['spindle-or']
        self.emit('spindle-override-changed',spindle_or_new  * 100)

        # spindle speed mpde
        css_new = self.old['css']
        if css_new:
            self.emit('css-mode',css_new)
        rpm_new = self.old['rpm']
        if rpm_new:
            self.emit('rpm-mode',rpm_new)

        # feed mode:
        itime_new = self.old['itime']
        if itime_new:
            self.emit('itime-mode',itime_new)
        fpm_new = self.old['fpm']
        if fpm_new:
            self.emit('fpm-mode',fpm_new)
        fpr_new = self.old['fpr']
        if fpr_new:
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
        if system_new:
            self.emit('user_system_changed',system_new)
        # radius mode g8
        radius_new = self.old['radius']
        self.emit('radius-mode',radius_new)
        # diameter mode g7
        diam_new = self.old['diameter']
        self.emit('diameter-mode',diam_new)
        # M codes
        m_code_new = self.old['m-code']
        self.emit('m-code-changed',m_code_new)
        # G codes
        g_code_new = self.old['g-code']
        self.emit('g-code-changed',g_code_new)
        # metric units G21
        metric_new = self.old['metric']
        if metric_new:
            self.emit('metric_mode_changed',metric_new)
        # tool in spindle
        tool_new = self.old['tool-in-spindle']
        self.emit('tool-in-spindle-changed', tool_new)

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

    def set_jog_rate(self,upm):
        self.current_jog_rate = upm
        self.emit('jograte-changed', upm)


    def is_all_homed(self):
        return self._is_all_homed

    def machine_is_on(self):
        return self.old['state']  > linuxcnc.STATE_OFF

    def estop_is_clear(self):
        return self.old['state'] > linuxcnc.STATE_ESTOP

    def is_auto_mode(self):
        self.stat.poll()
        print self.old['mode']  , linuxcnc.MODE_AUTO
        return self.old['state']  == linuxcnc.MODE_AUTO

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

#########
# Qstat
#########

class _QStat(QObject):
    '''Emits signals based on linuxcnc status '''
    widget_update = pyqtSignal()
    state_estop = pyqtSignal()
    state_estop_reset = pyqtSignal()
    state_on = pyqtSignal()
    state_off = pyqtSignal()
    homed = pyqtSignal(str)
    all_homed = pyqtSignal()
    not_all_homed = pyqtSignal(str)
    override_limits_changed = pyqtSignal(list)

    mode_manual = pyqtSignal()
    mode_auto = pyqtSignal()
    mode_mdi = pyqtSignal()

    interp_run = pyqtSignal()
    interp_idle = pyqtSignal()
    interp_paused = pyqtSignal()
    interp_reading = pyqtSignal()
    interp_waiting = pyqtSignal()
    jograte_changed = pyqtSignal(float)

    program_pause_changed = pyqtSignal(bool)
    optional_stop_changed = pyqtSignal(bool)
    block_delete_changed = pyqtSignal(bool)

    file_loaded = pyqtSignal(str)
    reload_display = pyqtSignal()
    line_changed = pyqtSignal(int)

    tool_in_spindle_changed = pyqtSignal(int)
    spindle_control_changed = pyqtSignal(int)

    current_feed_rate = pyqtSignal(float)
    current_x_rel_position = pyqtSignal(float)
    current_position = pyqtSignal(tuple,tuple,tuple)

    spindle_override_changed = pyqtSignal(float)
    feed_override_changed = pyqtSignal(float)
    rapid_override_changed = pyqtSignal(float)

    feed_hold_enabled_changed = pyqtSignal(bool)

    itime_mode = pyqtSignal(bool)
    fpm_mode = pyqtSignal(bool)
    fpr_mode = pyqtSignal(bool)
    css_mode = pyqtSignal(bool)
    rpm_mode = pyqtSignal(bool)
    radius_mode = pyqtSignal(bool)
    diameter_mode = pyqtSignal(bool)

    m_code_changed = pyqtSignal(str)
    g_code_changed = pyqtSignal(str)

    metric_mode_changed = pyqtSignal(bool)
    user_system_changed = pyqtSignal(int)

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
        self.current_jog_rate = 15

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

        self.old['paused']= self.stat.paused
        self.old['spindle_or'] = self.stat.spindlerate
        self.old['feed_or'] = self.stat.feedrate
        self.old['rapid_or'] = self.stat.rapidrate
        self.old['feed_hold']  = self.stat.feed_hold_enabled
        self.old['g5x_index']  = self.stat.g5x_index
        self.old['spindle_enabled']  = self.stat.spindle_enabled
        self.old['spindle_direction']  = self.stat.spindle_direction
        self.old['block_delete']= self.stat.block_delete
        self.old['optional_stop']= self.stat.optional_stop
        # override limits
        or_limit_list=[]
        for i in range(0,8):
            or_limit_list.append( self.stat.axis[i]['override_limits'])
        self.old['override_limits'] = or_limit_list
        # active G codes
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
        self.old['g_code'] = codes
        # extract specific G code modes
        itime = fpm = fpr = css = rpm = metric = False
        radius = diameter = False
        for num,i in enumerate(active_gcodes):
            if i == 'G93': itime = True
            elif i == 'G94': fpm = True
            elif i == 'G95': fpr = True
            elif i == 'G96': css = True
            elif i == 'G97': rpm = True
            elif i == 'G21': metric = True
            elif i == 'G7': diameter  = True
            elif i == 'G8': radius = True
        self.old['itime'] = itime
        self.old['fpm'] = fpm
        self.old['fpr'] = fpr
        self.old['css'] = css
        self.old['rpm'] = rpm
        self.old['metric'] = metric
        self.old['radius'] = radius
        self.old['diameter'] = diameter

        # active M codes
        active_mcodes = ''
        for i in sorted(self.stat.mcodes[1:]):
            if i == -1: continue
            active_mcodes = active_mcodes + ("M%s "%i)
            #active_mcodes.append("M%s "%i)
        self.old['m_code'] = active_mcodes

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



        # paused
        paused_old = old.get('paused', None)
        paused_new = self.old['paused']
        if paused_new != paused_old:
            self.program_pause_changed.emit(paused_new)
        # block delete
        block_delete_old = old.get('block_delete', None)
        block_delete_new = self.old['block_delete']
        if block_delete_new != block_delete_old:
            self.block_delete_changed.emit(block_delete_new)
        # optional_stop
        optional_stop_old = old.get('optionaL_stop', None)
        optional_stop_new = self.old['optional_stop']
        if optional_stop_new != optional_stop_old:
            self.optional_stop_changed.emit(optional_stop_new)



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

        # override limts
        or_limits_old = old.get('override_limits', None)
        or_limits_new = self.old['override_limits']
        if or_limits_new != or_limits_old:
            self.override_limits_changed.emit(or_limits_new)
        # current velocity
        self.current_feed_rate.emit(self.stat.current_vel * 60.0)
        # X relative position
        position = self.stat.actual_position[0]
        g5x_offset = self.stat.g5x_offset[0]
        tool_offset = self.stat.tool_offset[0]
        g92_offset = self.stat.g92_offset[0]
        self.current_x_rel_position.emit(position - g5x_offset - tool_offset - g92_offset)
        p,rel_p,dtg = self.get_position()
        self.current_position.emit(p, rel_p, dtg)

        # spindle control
        spindle_enabled_old = old.get('spindle_enabled', None)
        spindle_enabled_new = self.old['spindle_enabled']
        spindle_direction_old = old.get('spindle_direction', None)
        spindle_direction_new = self.old['spindle_direction']
        if spindle_enabled_new != spindle_enabled_old or spindle_direction_new != spindle_direction_old:
            self.spindle_control_changed.emit( spindle_enabled_new, spindle_direction_new)
        # spindle override
        spindle_or_old = old.get('spindle_or', None)
        spindle_or_new = self.old['spindle_or']
        if spindle_or_new != spindle_or_old:
            self.spindle_override_changed.emit(spindle_or_new * 100)
        # feed override
        feed_or_old = old.get('feed_or', None)
        feed_or_new = self.old['feed_or']
        if feed_or_new != feed_or_old:
            self.feed_override_changed.emit(feed_or_new * 100)
        # rapid override
        rapid_or_old = old.get('rapid_or', None)
        rapid_or_new = self.old['rapid_or']
        if rapid_or_new != rapid_or_old:
            self.rapid_override_changed.emit(rapid_or_new * 100)
        # feed hold
        feed_hold_old = old.get('feed_hold', None)
        feed_hold_new = self.old['feed_hold']
        if feed_hold_new != feed_hold_old:
            self.feed_hold_enabled_changed.emit(feed_hold_new)
        # G5x (active user system)
        g5x_index_old = old.get('g5x_index', None)
        g5x_index_new = self.old['g5x_index']
        if g5x_index_new != g5x_index_old:
            self.user_system_changed.emit(g5x_index_new)
        # inverse time mode g93
        itime_old = old.get('itime', None)
        itime_new = self.old['itime']
        if itime_new != itime_old:
            self.itime_mode.emit(itime_new)




        # feed per minute mode g94
        fpm_old = old.get('fpm', None)
        fpm_new = self.old['fpm']
        if fpm_new != fpm_old:
            self.fpm_mode.emit(fpm_new)
        # feed per revolution mode g95
        fpr_old = old.get('fpr', None)
        fpr_new = self.old['fpr']
        if fpr_new != fpr_old:
            self.fpr_mode.emit(fpr_new)
        # css mode g96
        css_old = old.get('css', None)
        css_new = self.old['css']
        if css_new != css_old:
            self.css_mode.emit(css_new)
        # rpm mode g97
        rpm_old = old.get('rpm', None)
        rpm_new = self.old['rpm']
        if rpm_new != rpm_old:
            self.rpm_mode.emit(rpm_new)
        # radius mode g8
        radius_old = old.get('radius', None)
        radius_new = self.old['radius']
        if radius_new != radius_old:
            self.radius_mode.emit(radius_new)
        # diameter mode g7
        diam_old = old.get('diameter', None)
        diam_new = self.old['diameter']
        if diam_new != diam_old:
            self.diameter_mode.emit(diam_new)
        # M codes
        m_code_old = old.get('m_code', None)
        m_code_new = self.old['m_code']
        if m_code_new != m_code_old:
            self.m_code_changed.emit(m_code_new)
        # G codes
        g_code_old = old.get('g_code', None)
        g_code_new = self.old['g_code']
        if g_code_new != g_code_old:
            self.g_code_changed.emit(g_code_new)
        # metric mode g21
        metric_old = old.get('metric', None)
        metric_new = self.old['metric']
        if metric_new != metric_old:
            self.metric_mode_changed.emit(metric_new)
        # A widget can register for an update signal ever 100ms
        self.widget_update.emit()


    def forced_update(self):
        print 'forced!'
        try:
            self.stat.poll()
        except:
            # Reschedule
            return True
        self.merge()
        self.jograte_changed.emit(15)
        # override limts
        or_limits_new = self.old['override_limits']
        print 'override',or_limits_new
        self.override_limits_changed.emit(or_limits_new)
        # overrides
        feed_or_new = self.old['feed_or']
        self.feed_override_changed.emit(feed_or_new * 100)
        rapid_or_new = self.old['rapid_or']
        self.rapid_override_changed.emit(rapid_or_new  * 100)
        spindle_or_new = self.old['spindle_or']
        self.spindle_override_changed.emit(spindle_or_new  * 100)

        # spindle speed mpde
        css_new = self.old['css']
        if css_new:
            self.css_mode.emit(css_new)
        rpm_new = self.old['rpm']
        if rpm_new:
            self.rpm_mode.emit(rpm_new)

        # feed mode:
        itime_new = self.old['itime']
        if itime_new:
            self.itime_mode.emit(itime_new)
        fpm_new = self.old['fpm']
        if fpm_new:
            self.fpm_mode.emit(fpm_new)
        fpr_new = self.old['fpr']
        if fpr_new:
            self.fpr_mode.emit(fpr_new)
        # paused
        paused_new = self.old['paused']
        self.program_pause_changed.emit(paused_new)
        # block delete
        block_delete_new = self.old['block_delete']
        self.block_delete_changed.emit(block_delete_new)
        # optional_stop
        optional_stop_new = self.old['optional_stop']
        self.optional_stop_changed.emit(optional_stop_new)
        # user system G5x
        system_new = self.old['g5x_index']
        if system_new:
            self.user_system_changed.emit(system_new)
        # radius mode g8
        radius_new = self.old['radius']
        self.radius_mode.emit(radius_new)
        # diameter mode g7
        diam_new = self.old['diameter']
        self.diameter_mode.emit(diam_new)
        # M codes
        m_code_new = self.old['m_code']
        self.m_code_changed.emit(m_code_new)
        # G codes
        g_code_new = self.old['g_code']
        self.g_code_changed.emit(g_code_new)
        # metric units G21
        metric_new = self.old['metric']
        if metric_new:
            self.metric_mode_changed.emit(metric_new)
        # tool in spindle
        tool_new = self.old['tool_in_spindle']
        self.tool_in_spindle_changed.emit( tool_new)

    def get_position(self):
        p = self.stat.actual_position
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

    def set_jog_rate(self,upm):
        self.current_jog_rate = upm
        self.jograte_changed.emit(upm)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class QStat(_QStat):
    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _QStat.__new__(cls, *args, **kwargs)
            print 'new', cls._instance
        else: print 'old', cls._instance
        return cls._instance
