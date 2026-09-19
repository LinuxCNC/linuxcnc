"""Pure Python state machine for a hydraulic single-solenoid lathe turret.

Machine profile (configurable):
  * one solenoid: energized = rotate in one direction, de-energized =
    the mechanism seats the turret on a station,
  * sensors: ``pos1`` (station 1 / index), ``strobe`` (pulse per
    station), ``changepos`` (turret seated, clamp allowed) and
    ``clamped`` (clamp closed),
  * clamp valve: one solenoid, ON = unclamp (spring closes).

The FSM has an explicit timeout and a fault code for every transition,
adapts the "stop lead" to the hydraulic coast, detects spurious strobe
pulses while stopped and keeps counters for maintenance.

This module has no HAL dependency: it is driven by ``update(now,
inputs)`` and unit tested directly.
"""

import math
from dataclasses import dataclass, field
from enum import IntEnum

from .codes import Alarm


class State(IntEnum):
    IDLE = 0
    UNCLAMP = 1
    HOME = 2
    ROTATE = 3
    SETTLE = 4
    LOCATE = 5
    CLAMP = 6
    FAULT = 10
    RELEASE = 12
    JOG = 13


STATE_TEXT = {
    State.IDLE: "Reposo",
    State.UNCLAMP: "Liberando pinza",
    State.HOME: "Buscando referencia",
    State.ROTATE: "Girando",
    State.SETTLE: "Asentando",
    State.LOCATE: "Verificando posicion",
    State.CLAMP: "Cerrando pinza",
    State.FAULT: "Falla",
    State.RELEASE: "Pinza liberada (servicio)",
    State.JOG: "Avance manual",
}

_MOVING_STATES = (State.UNCLAMP, State.HOME, State.ROTATE, State.SETTLE,
                  State.LOCATE, State.CLAMP, State.JOG)


@dataclass
class Inputs:
    pos1: bool = False
    strobe: bool = False
    changepos: bool = False
    clamped: bool = False
    interlock_ok: bool = True
    test_enable: bool = False
    jog: bool = False


@dataclass
class Outputs:
    motor: bool = False
    unclamp: bool = False
    clamp: bool = False
    state: int = int(State.IDLE)
    station: int = 1
    busy: bool = False
    homed: bool = False
    located: bool = False
    fault: bool = False
    error: int = 0


def wrap_station(value, stations):
    return ((value - 1) % stations) + 1


class TurretFSM:
    """Deterministic turret controller; feed it time and inputs."""

    def __init__(self, stations=8, timing=None, logic=None):
        self.stations = int(stations)
        self.timing = timing
        self.logic = logic

        # persistent-ish state
        self.state = State.IDLE
        self.station = 1
        self.homed = False
        self.located = False
        self.fault = False
        self.error = int(Alarm.NONE)
        self.lead = int(getattr(timing, "lead_pulses", 0) or 0)
        self.last_fault = int(Alarm.NONE)
        self.last_fault_time = None

        # counters (maintenance)
        self.changes = 0
        self.clamp_cycles = 0
        self.fault_count = 0
        self.station_counts = [0] * self.stations

        # requests
        self._req_select = None
        self._req_home = False
        self._req_reset = False
        self._req_release = False
        self._req_lock = False
        self._req_step = False
        self._req_jog = False
        self._req_abort = False

        # sequence bookkeeping
        self._seq = None            # 'select' | 'home' | 'release'
        self._target = None
        self._after_unclamp = None  # 'home' | 'rotate' | 'release'
        self._settle_mode = None
        self._rot_base = 1
        self._rot_counted = 0
        self._rot_target_pulses = 0
        self._rot_stopped_count = 0
        self._retries = 0
        self._coast_history = []
        self._jog_count = 0
        self._need_rehome = False
        self.last_completed = 0

        # timing
        self._t0 = None
        self._strobe_prev = False
        self._strobe_time = None
        self._pos1_prev = False
        self._pos1_time = None
        self._locate_since = None
        self._idle_strobes = 0

    # ------------------------------------------------------------------
    # requests
    # ------------------------------------------------------------------
    def request_select(self, pocket):
        pocket = int(pocket)
        if 1 <= pocket <= self.stations:
            self._req_select = pocket

    def request_home(self):
        self._req_home = True

    def request_reset(self):
        self._req_reset = True

    def request_release(self):
        self._req_release = True

    def request_lock(self):
        self._req_lock = True

    def request_step(self):
        self._req_step = True

    def request_jog(self):
        self._req_jog = True

    def request_abort(self):
        self._req_abort = True

    # ------------------------------------------------------------------
    # helpers
    # ------------------------------------------------------------------
    @property
    def busy(self):
        return self.state in _MOVING_STATES

    @property
    def at_target(self):
        return (self.state == State.IDLE and self._target is not None
                and self.station == self._target and self.located)

    def _clamped(self, inp):
        return (not inp.clamped) if self.logic.clamped_inverted else bool(inp.clamped)

    def _elapsed(self, now):
        return 0.0 if self._t0 is None else (now - self._t0)

    def _enter(self, state, now):
        self.state = state
        self._t0 = now
        if state == State.LOCATE:
            self._locate_since = None

    def _set_fault(self, code, now=None):
        self.state = State.FAULT
        self.fault = True
        self.error = int(code)
        self.last_fault = int(code)
        self.last_fault_time = now
        self.fault_count += 1
        self._need_rehome = True
        self._seq = None
        self._target = None
        self._req_select = None

    def _tick_strobe(self, now, inp):
        """Return True on a debounced rising edge of strobe."""
        edge = False
        if inp.strobe and not self._strobe_prev:
            gap = self.timing.strobe_filter_ms / 1000.0
            if self._strobe_time is None or (now - self._strobe_time) >= gap:
                edge = True
                self._strobe_time = now
        self._strobe_prev = bool(inp.strobe)
        return edge

    def _tick_pos1(self, now, inp):
        edge = False
        if inp.pos1 and not self._pos1_prev:
            gap = self.timing.strobe_filter_ms / 1000.0
            if self._pos1_time is None or (now - self._pos1_time) >= gap:
                edge = True
                self._pos1_time = now
        self._pos1_prev = bool(inp.pos1)
        return edge

    def _adapt_lead(self, coast):
        if not self.timing.lead_auto:
            return
        self._coast_history.append(max(0, int(coast)))
        self._coast_history = self._coast_history[-5:]
        ordered = sorted(self._coast_history)
        median = ordered[len(ordered) // 2]
        self.lead = max(0, min(2, int(median)))

    def _enter_rotate(self, now):
        self._rot_base = self.station
        self._rot_counted = 0
        pulses = (self._target - self._rot_base) % self.stations
        self._rot_target_pulses = pulses
        if pulses == 0:
            self._enter(State.LOCATE, now)
            return
        self._enter(State.ROTATE, now)

    # ------------------------------------------------------------------
    # main entry point
    # ------------------------------------------------------------------
    def update(self, now, inp):
        strobe_edge = self._tick_strobe(now, inp)
        pos1_edge = self._tick_pos1(now, inp)

        if self._req_abort:
            self._req_abort = False
            if self.state != State.FAULT:
                self._set_fault(Alarm.E_OPERATOR_ABORT, now)

        if (self.logic.interlock_required and not inp.interlock_ok
                and self.state in _MOVING_STATES):
            self._set_fault(Alarm.E_INTERLOCK, now)

        handler = getattr(self, "_st_" + State(self.state).name.lower())
        handler(now, inp, strobe_edge, pos1_edge)

        return self._outputs()

    def _outputs(self):
        out = Outputs()
        out.state = int(self.state)
        out.station = self.station
        out.busy = self.busy
        out.homed = self.homed
        out.located = self.located
        out.fault = self.fault
        out.error = self.error
        if self.state == State.FAULT:
            out.motor = False
            out.unclamp = False
        elif self.state == State.RELEASE:
            out.motor = False
            out.unclamp = True
        elif self.state in (State.UNCLAMP, State.HOME, State.ROTATE,
                            State.SETTLE, State.LOCATE, State.CLAMP,
                            State.JOG):
            out.motor = self.state in (State.HOME, State.ROTATE, State.JOG)
            out.unclamp = self.state != State.CLAMP
        return out

    # ------------------------------------------------------------------
    # states
    # ------------------------------------------------------------------
    def _st_idle(self, now, inp, strobe_edge, pos1_edge):
        if strobe_edge:
            self._idle_strobes += 1
            if self._idle_strobes > 3:
                self._set_fault(Alarm.E_STROBE_FAULT, now)
                return
        if self._req_release and self._test_ok(inp):
            self._req_release = False
            self._seq = "release"
            self._after_unclamp = "release"
            self._enter(State.UNCLAMP, now)
            return
        if self._req_jog:
            self._req_jog = False
            if self._test_ok(inp):
                self.located = False
                self._seq = "jog"
                self._jog_count = 0
                self._idle_strobes = 0
                if self._clamped(inp):
                    self._after_unclamp = "jog"
                    self._enter(State.UNCLAMP, now)
                else:
                    self._enter(State.JOG, now)
                return
        if self._req_home:
            self._req_home = False
            self.located = False
            self._seq = "home"
            self._after_unclamp = "home"
            self._enter(State.UNCLAMP, now)
            return
        pocket = self._req_select
        if pocket is None and self._req_step and self._test_ok(inp):
            self._req_step = False
            pocket = wrap_station(self.station + 1, self.stations)
        if pocket is not None:
            self._req_select = None
            self._req_step = False
            self.located = False
            self._target = pocket
            self._seq = "select"
            self._retries = 0
            self._idle_strobes = 0
            self._after_unclamp = "rotate" if self.homed else "home"
            self._enter(State.UNCLAMP, now)

    def _test_ok(self, inp):
        return inp.test_enable or not self.logic.test_enable_required

    def _st_unclamp(self, now, inp, strobe_edge, pos1_edge):
        if not self._clamped(inp):
            if self._after_unclamp == "release":
                self._enter(State.RELEASE, now)
            elif self._after_unclamp == "home":
                self._enter(State.HOME, now)
            elif self._after_unclamp == "jog":
                self._enter(State.JOG, now)
            else:
                self._enter_rotate(now)
            return
        if self._elapsed(now) > self.timing.unclamp_timeout:
            self._set_fault(Alarm.E_UNCLAMP_TIMEOUT, now)

    def _st_home(self, now, inp, strobe_edge, pos1_edge):
        if pos1_edge and inp.strobe:
            self._rot_counted = 0
            self._settle_mode = "home"
            self._enter(State.SETTLE, now)
            return
        if self._elapsed(now) > self.timing.home_timeout:
            self._set_fault(Alarm.E_HOME_TIMEOUT, now)

    def _st_rotate(self, now, inp, strobe_edge, pos1_edge):
        if strobe_edge:
            self._rot_counted += 1
        stop_at = max(1, self._rot_target_pulses - self.lead)
        if self._rot_counted >= stop_at:
            self._rot_stopped_count = self._rot_counted
            self._settle_mode = "rotate"
            self._enter(State.SETTLE, now)
            return
        if self._elapsed(now) > self.timing.rotate_timeout:
            self._set_fault(Alarm.E_ROTATE_TIMEOUT, now)

    def _st_settle(self, now, inp, strobe_edge, pos1_edge):
        if strobe_edge:
            if self._settle_mode == "jog":
                self._jog_count += 1
                if self.homed:
                    self.station = wrap_station(self.station + 1, self.stations)
            else:
                self._rot_counted += 1
        if self._elapsed(now) < self.timing.settle_time:
            return
        if self._settle_mode == "jog":
            self._enter(State.LOCATE, now)
            return
        if self._settle_mode == "home":
            self.station = wrap_station(1 + self._rot_counted, self.stations)
            self.homed = True
            self._idle_strobes = 0
            if self._seq == "select" and self.station != self._target:
                self._enter_rotate(now)
            else:
                self._enter(State.LOCATE, now)
            return
        # rotate settle
        coast = self._rot_counted - self._rot_stopped_count
        self._adapt_lead(coast)
        new_station = wrap_station(self._rot_base + self._rot_counted, self.stations)
        self.station = new_station
        if self._rot_counted > self._rot_target_pulses:
            self._set_fault(Alarm.E_OVERSHOOT, now)
            return
        if new_station != self._target:
            if self._retries < self.timing.max_retries:
                self._retries += 1
                self._enter_rotate(now)
            else:
                self._set_fault(Alarm.E_ROTATE_TIMEOUT, now)
            return
        self._enter(State.LOCATE, now)

    def _st_jog(self, now, inp, strobe_edge, pos1_edge):
        if pos1_edge and inp.strobe:
            # reaching station 1 manually is a valid reference
            self.station = 1
            self.homed = True
            self.located = False
            self._jog_count = 0
            self._idle_strobes = 0
        elif strobe_edge:
            self._jog_count += 1
            if self.homed:
                self.station = wrap_station(self.station + 1, self.stations)
        hold = bool(inp.jog)
        if not hold or self._elapsed(now) > self.timing.jog_timeout:
            self._settle_mode = "jog"
            self._enter(State.SETTLE, now)

    def _st_locate(self, now, inp, strobe_edge, pos1_edge):
        if not self.logic.require_changepos:
            self._enter(State.CLAMP, now)
            return
        if inp.changepos:
            if self._locate_since is None:
                self._locate_since = now
            elif (now - self._locate_since) * 1000.0 >= self.timing.locate_stable_ms:
                self._enter(State.CLAMP, now)
                return
        else:
            self._locate_since = None
        if self._elapsed(now) > self.timing.locate_timeout:
            self._set_fault(Alarm.E_NOT_LOCATED, now)

    def _st_clamp(self, now, inp, strobe_edge, pos1_edge):
        if self._clamped(inp):
            self.clamp_cycles += 1
            self.located = True
            if self._seq == "select":
                self.changes += 1
                self.last_completed = self.station
                self.station_counts[self.station - 1] += 1
            self._seq = None
            self._retries = 0
            self._idle_strobes = 0
            self._enter(State.IDLE, now)
            return
        if self._elapsed(now) > self.timing.clamp_timeout:
            self._set_fault(Alarm.E_CLAMP_TIMEOUT, now)

    def _st_release(self, now, inp, strobe_edge, pos1_edge):
        if self._req_lock:
            self._req_lock = False
            self._seq = None
            self._enter(State.CLAMP, now)
            return
        if self._req_home:
            self._req_home = False
            self.located = False
            self._seq = "home"
            self._enter(State.HOME, now)

    def _st_fault(self, now, inp, strobe_edge, pos1_edge):
        if self._req_reset:
            self._req_reset = False
            if self.homed and self._need_rehome:
                self.homed = False
            self._need_rehome = False
            self.fault = False
            self.error = int(Alarm.NONE)
            self._enter(State.IDLE, now)

    # ------------------------------------------------------------------
    # state export for persistence / GUI
    # ------------------------------------------------------------------
    def state_dict(self):
        return {
            "station": self.station,
            "homed": self.homed,
            "lead": self.lead,
            "changes": self.changes,
            "clamp_cycles": self.clamp_cycles,
            "fault_count": self.fault_count,
            "station_counts": list(self.station_counts),
        }

    def load_state(self, data):
        if not isinstance(data, dict):
            return
        self.station = wrap_station(int(data.get("station", 1)), self.stations)
        self.homed = bool(data.get("homed", False))
        self.lead = max(0, min(2, int(data.get("lead", self.lead))))
        self.changes = int(data.get("changes", 0))
        self.clamp_cycles = int(data.get("clamp_cycles", 0))
        self.fault_count = int(data.get("fault_count", 0))
        counts = data.get("station_counts", [])
        if isinstance(counts, list) and len(counts) == self.stations:
            self.station_counts = [int(c) for c in counts]
