#!/usr/bin/env python3
"""Unit tests for the turret FSM (pure Python, no HAL)."""

import os
import sys
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "lib", "python"))

from turret.config import Logic, Timing
from turret.codes import Alarm
from turret.fsm import Inputs, State, TurretFSM


class Plant:
    """Very small model of the hydraulic turret for the tests."""

    def __init__(self, stations=8, start_station=2, pulse_dt=0.1,
                 coast_pulses=0, clamped=True, unclamp_delay=0.0,
                 clamp_delay=0.0):
        self.stations = stations
        self.station = start_station
        self.t = 0.0
        self.dt = 0.01
        self.pulse_dt = pulse_dt
        self.coast_pulses = coast_pulses
        self.unclamp_delay = unclamp_delay
        self.clamp_delay = clamp_delay
        self.clamped = clamped
        self.motor = False
        self.unclamp = False
        self.strobe = False
        self._pulse_next = None
        self._pulse_until = -1.0
        self._coast_left = 0
        self._change_since = None
        self._unclamp_since = None
        self._clamp_since = None

    def inputs(self, interlock_ok=True, test_enable=True, jog=False):
        return Inputs(
            pos1=(self.station == 1),
            strobe=self.strobe,
            changepos=self._located(),
            clamped=self.clamped,
            interlock_ok=interlock_ok,
            test_enable=test_enable,
            jog=jog,
        )

    def _located(self):
        if self.motor or self._coast_left > 0:
            return False
        if self._change_since is None:
            self._change_since = self.t
        return (self.t - self._change_since) >= 0.05

    def apply(self, out):
        if out.unclamp:
            if self._unclamp_since is None:
                self._unclamp_since = self.t
            if (self.t - self._unclamp_since) >= self.unclamp_delay:
                self.clamped = False
        else:
            self._unclamp_since = None
            if self._clamp_since is None:
                self._clamp_since = self.t
            if (self.t - self._clamp_since) >= self.clamp_delay:
                self.clamped = True
        if out.motor and not self.motor:
            self._pulse_next = self.t + self.pulse_dt
        if not out.motor and self.motor:
            self._coast_left = self.coast_pulses
            self._pulse_next = self.t + self.pulse_dt if self._coast_left else None
        self.motor = out.motor

    def step(self):
        self.t += self.dt
        if self._pulse_until >= 0 and self.t >= self._pulse_until:
            self.strobe = False
            self._pulse_until = -1.0
        if self._pulse_next is not None and self.t >= self._pulse_next:
            self.strobe = True
            self._pulse_until = self.t + self.dt / 2.0
            self.station = ((self.station - 1 + 1) % self.stations) + 1
            self._change_since = None
            if self.motor:
                self._pulse_next = self.t + self.pulse_dt
            elif self._coast_left > 0:
                self._coast_left -= 1
                self._pulse_next = (self.t + self.pulse_dt
                                    if self._coast_left else None)
            else:
                self._pulse_next = None


def run(fsm, plant, until, max_time=30.0, **kw):
    while plant.t < max_time:
        out = fsm.update(plant.t, plant.inputs(**kw))
        plant.apply(out)
        if until(out, fsm):
            return out
        plant.step()
    return None


class TestTurretFSM(unittest.TestCase):

    def make(self, **kw):
        timing = Timing(settle_time=0.25, locate_stable_ms=50.0,
                        unclamp_timeout=1.0, clamp_timeout=1.0,
                        rotate_timeout=2.0, home_timeout=3.0,
                        locate_timeout=1.0)
        for key, value in kw.items():
            setattr(timing, key, value)
        return TurretFSM(8, timing, Logic()), timing

    # ------------------------------------------------------------------
    def test_select_from_homed(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1)
        fsm.request_select(3)
        out = run(fsm, plant, lambda o, f: o.state == int(State.IDLE) and f.changes == 1)
        self.assertIsNotNone(out, "no completo la secuencia")
        self.assertEqual(fsm.station, 3)
        self.assertEqual(fsm.changes, 1)
        self.assertEqual(fsm.clamp_cycles, 1)
        self.assertEqual(fsm.station_counts[2], 1)
        self.assertTrue(fsm.located)

    def test_home_without_test_enable(self):
        fsm, _ = self.make()
        plant = Plant(start_station=5)
        fsm.request_home()
        out = run(fsm, plant, lambda o, f: o.state == int(State.IDLE)
                  and f.homed, test_enable=False)
        self.assertIsNotNone(out, "referenciar debe funcionar sin modo servicio")
        self.assertEqual(fsm.station, 1)

    def test_jog_hold_and_stop(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1)
        fsm.request_jog()
        out = run(fsm, plant, lambda o, f: f.state == State.JOG, jog=True)
        self.assertIsNotNone(out)
        self.assertTrue(out.motor)
        for _ in range(15):
            out = fsm.update(plant.t, plant.inputs(jog=True))
            plant.apply(out)
            plant.step()
        self.assertNotEqual(plant.station, 1)
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE, jog=False)
        self.assertIsNotNone(out)
        self.assertFalse(out.motor)
        self.assertTrue(plant.clamped)

    def test_jog_reaches_reference(self):
        fsm, _ = self.make()
        plant = Plant(start_station=5)
        self.assertFalse(fsm.homed)
        fsm.request_jog()
        while plant.t < 10.0:
            inp = plant.inputs(jog=True)
            inp.jog = not (inp.pos1 and inp.strobe)
            out = fsm.update(plant.t, inp)
            plant.apply(out)
            if out.state == int(State.IDLE) and fsm.homed:
                break
            plant.step()
        self.assertTrue(fsm.homed, "el avance manual debe referenciar al pasar por 1")
        self.assertEqual(fsm.station, 1)

    def test_jog_from_release(self):
        fsm, _ = self.make()
        plant = Plant(start_station=1)
        fsm.request_release()
        out = run(fsm, plant, lambda o, f: f.state == State.RELEASE)
        self.assertIsNotNone(out)
        fsm.request_jog()
        out = run(fsm, plant, lambda o, f: f.state == State.JOG, jog=True)
        self.assertIsNotNone(out)
        self.assertTrue(out.motor)
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE, jog=False)
        self.assertIsNotNone(out)
        self.assertTrue(plant.clamped)

    def test_jog_at_position1_references(self):
        fsm, _ = self.make()
        plant = Plant(start_station=1)
        self.assertFalse(fsm.homed)
        fsm.request_jog()
        while plant.t < 5.0:
            out = fsm.update(plant.t, plant.inputs(jog=True))
            plant.apply(out)
            if fsm.state == State.JOG:
                break
            plant.step()
        self.assertEqual(fsm.state, State.JOG)
        self.assertTrue(fsm.homed, "si ya esta en la 1, el jog debe referenciar")
        self.assertEqual(fsm.station, 1)

    def test_select_homes_first(self):
        fsm, _ = self.make()
        plant = Plant(start_station=2)
        fsm.request_select(5)
        out = run(fsm, plant, lambda o, f: o.state == int(State.IDLE) and f.changes == 1)
        self.assertIsNotNone(out, "no completo la secuencia")
        self.assertTrue(fsm.homed)
        self.assertEqual(fsm.station, 5)

    def test_unclamp_timeout(self):
        fsm, _ = self.make()
        fsm.homed = True
        plant = Plant(start_station=1, clamped=True)
        # never unclamp
        plant.apply = lambda out: None
        fsm.request_select(2)
        out = run(fsm, plant, lambda o, f: f.fault)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.error, int(Alarm.E_UNCLAMP_TIMEOUT))
        self.assertEqual(fsm.state, State.FAULT)

    def test_rotate_timeout(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1)
        plant.pulse_dt = 1000.0     # never pulses
        fsm.request_select(3)
        out = run(fsm, plant, lambda o, f: f.fault)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.error, int(Alarm.E_ROTATE_TIMEOUT))

    def test_overshoot(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1, coast_pulses=3)
        fsm.request_select(3)
        out = run(fsm, plant, lambda o, f: f.fault)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.error, int(Alarm.E_OVERSHOOT))

    def test_clamp_timeout(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1, clamp_delay=1000.0)
        fsm.request_select(2)
        out = run(fsm, plant, lambda o, f: f.fault)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.error, int(Alarm.E_CLAMP_TIMEOUT))

    def test_spurious_strobe(self):
        fsm, _ = self.make()
        plant = Plant(start_station=1)

        def until(o, f):
            return f.fault

        def poke():
            for _ in range(6):
                plant.strobe = True
                fsm.update(plant.t, plant.inputs())
                plant.step()
                plant.strobe = False
                fsm.update(plant.t, plant.inputs())
                plant.step()
        poke()
        self.assertEqual(fsm.error, int(Alarm.E_STROBE_FAULT))

    def test_interlock_lost(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 1
        fsm.logic.interlock_required = True
        plant = Plant(start_station=1)
        fsm.request_select(3)
        out = run(fsm, plant, lambda o, f: f.fault, interlock_ok=False)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.error, int(Alarm.E_INTERLOCK))

    def test_abort(self):
        fsm, _ = self.make()
        fsm.homed = True
        plant = Plant(start_station=1)
        fsm.request_select(4)
        run(fsm, plant, lambda o, f: f.state == State.ROTATE)
        fsm.request_abort()
        out = fsm.update(plant.t, plant.inputs())
        self.assertEqual(out.state, int(State.FAULT))
        self.assertEqual(fsm.error, int(Alarm.E_OPERATOR_ABORT))
        self.assertFalse(out.motor)
        self.assertFalse(out.unclamp)

    def test_reset_from_fault(self):
        fsm, _ = self.make()
        fsm.homed = True
        plant = Plant(start_station=1)
        plant.pulse_dt = 1000.0
        fsm.request_select(3)
        run(fsm, plant, lambda o, f: f.fault)
        fsm.request_reset()
        plant.pulse_dt = 0.1
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE)
        self.assertIsNotNone(out)
        self.assertFalse(fsm.fault)
        self.assertFalse(fsm.homed, "reset tras falla debe exigir referencia")

    def test_release_and_lock(self):
        fsm, _ = self.make()
        plant = Plant(start_station=1)
        fsm.request_release()
        out = run(fsm, plant, lambda o, f: f.state == State.RELEASE)
        self.assertIsNotNone(out)
        self.assertTrue(out.unclamp)
        self.assertFalse(out.motor)
        fsm.request_lock()
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE)
        self.assertIsNotNone(out)
        self.assertFalse(out.unclamp)
        self.assertTrue(plant.clamped)

    def test_reference_when_homed(self):
        fsm, _ = self.make()
        fsm.homed = True
        fsm.station = 5
        plant = Plant(start_station=5)
        fsm.request_home()
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.station, 1)
        self.assertTrue(fsm.homed)

    def test_lead_adapts_to_coast(self):
        # started with the configured lead of one pulse
        fsm, _ = self.make(lead_pulses=1)
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1, coast_pulses=1)
        fsm.request_select(4)
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE and f.changes == 1)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.station, 4)
        self.assertEqual(fsm.lead, 1)
        # second move must still land on target with the learned lead
        fsm.request_select(2)
        out = run(fsm, plant, lambda o, f: f.state == State.IDLE and f.changes == 2)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.station, 2)

    def test_zero_lead_overshoots_on_coast(self):
        # without lead the first move overruns: it must fault, not clamp
        fsm, _ = self.make(lead_pulses=0)
        fsm.homed = True
        fsm.station = 1
        plant = Plant(start_station=1, coast_pulses=1)
        fsm.request_select(4)
        out = run(fsm, plant, lambda o, f: f.fault)
        self.assertIsNotNone(out)
        self.assertEqual(fsm.error, int(Alarm.E_OVERSHOOT))

    def test_state_roundtrip(self):
        fsm, _ = self.make()
        fsm.station = 6
        fsm.homed = True
        fsm.changes = 42
        fsm.station_counts[5] = 7
        data = fsm.state_dict()
        other, _ = self.make()
        other.load_state(data)
        self.assertEqual(other.station, 6)
        self.assertTrue(other.homed)
        self.assertEqual(other.changes, 42)
        self.assertEqual(other.station_counts[5], 7)


if __name__ == "__main__":
    unittest.main(verbosity=2)
