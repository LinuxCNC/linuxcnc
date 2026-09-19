#!/usr/bin/env python3
"""Glue tests for the turret HAL manager using a fake HAL module."""

import json
import os
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "lib", "python"))

from turret.codes import Alarm
from turret.config import TurretConfig, Interval
from turret.fsm import State
from turret.manager import TurretManager


class FakeHal:
    class Type:
        BIT = "bit"
        S32 = "s32"
        FLOAT = "float"

    class Dir:
        IN = "in"
        OUT = "out"
        IO = "io"


class FakePin:
    def __init__(self, value=0):
        self.value = value


class FakeIO:
    """Minimal stand-in for HalIO used by the manager tests."""

    def __init__(self):
        self.hal = FakeHal()
        self.pins = {}
        self.params = {}
        self.external = {}
        self.ready_called = False

    def newpin(self, name, kind, direction):
        self.pins[name] = FakePin(0)
        return self.pins[name]

    def newparam(self, name, kind, direction):
        self.params[name] = FakePin(0)
        return self.params[name]

    def param(self, name):
        return self.params[name].value

    def param_set(self, name, value):
        self.params[name].value = value

    def ready(self):
        self.ready_called = True

    def own(self, name):
        return self.pins[name].value

    def own_set(self, name, value):
        self.pins[name].value = value

    def get(self, name):
        return self.external.get(name, 0)

    def set(self, name, value):
        self.external[name] = value


class SimMachine:
    """Sensor model driven by the motor/unclamp pins of the manager."""

    def __init__(self, io, stations=8, start=2, pulse_dt=0.1, coast=0,
                 clamp_delay=0.0):
        self.io = io
        self.stations = stations
        self.station = start
        self.pulse_dt = pulse_dt
        self.coast = coast
        self.clamp_delay = clamp_delay
        self.t = 0.0
        self.dt = 0.01
        self._pulse_next = None
        self._coast_left = 0
        self._motor_prev = False
        self._unclamp_prev = False
        self._clamped = True
        self._unclamp_since = None

    def step(self):
        io = self.io
        motor = bool(io.external.get("sim.motor", 0))
        unclamp = bool(io.external.get("sim.unclamp", 0))
        if unclamp and not self._unclamp_prev:
            self._unclamp_since = self.t
        if not unclamp:
            self._unclamp_since = None
            if self._clamped is False and self.clamp_delay == 0:
                self._clamped = True
        if unclamp and self._unclamp_since is not None:
            if (self.t - self._unclamp_since) >= 0.0:
                self._clamped = False
        if motor and not self._motor_prev:
            self._pulse_next = self.t + self.pulse_dt
        if not motor and self._motor_prev:
            self._coast_left = self.coast
            self._pulse_next = self.t + self.pulse_dt if self.coast else None
        self._motor_prev = motor
        self._unclamp_prev = unclamp

        self.t += self.dt
        strobe = False
        if self._pulse_next is not None and self.t >= self._pulse_next:
            strobe = True
            self.station = ((self.station - 1 + 1) % self.stations) + 1
            if motor:
                self._pulse_next = self.t + self.pulse_dt
            elif self._coast_left > 0:
                self._coast_left -= 1
                self._pulse_next = (self.t + self.pulse_dt
                                    if self._coast_left else None)
            else:
                self._pulse_next = None
        io.external["sim.pos1"] = 1 if self.station == 1 else 0
        io.external["sim.strobe"] = 1 if strobe else 0
        io.external["sim.changepos"] = 0 if (motor or self._coast_left) else 1
        io.external["sim.clamped"] = 1 if self._clamped else 0


def make_config(tmp, stations=8, coast=0):
    cfg = TurretConfig()
    cfg.stations = stations
    cfg.pins = {
        "pos1": "sim.pos1",
        "strobe": "sim.strobe",
        "changepos": "sim.changepos",
        "clamped": "sim.clamped",
        "motor": "sim.motor",
        "unclamp": "sim.unclamp",
        "tool_prepare": "iocontrol.0.tool-prepare",
        "tool_prepared": "iocontrol.0.tool-prepared",
        "tool_change": "iocontrol.0.tool-change",
        "tool_changed": "iocontrol.0.tool-changed",
        "tool_prep_pocket": "iocontrol.0.tool-prep-pocket",
        "toolchanger_fault": "iocontrol.0.toolchanger-fault",
        "toolchanger_reason": "iocontrol.0.toolchanger-reason",
    }
    cfg.timing.settle_time = 0.25
    cfg.timing.lead_pulses = 1
    cfg.maintenance = [Interval(code=1, name="Pinza", every_changes=2)]
    path = os.path.join(tmp, "turret.json")
    cfg.save(path)
    return path


def run_until(manager, machine, until, max_time=30.0):
    while machine.t < max_time:
        manager._tick(machine.t)
        machine.step()
        if until(manager, machine):
            return True
    return False


class TestManager(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = make_config(self.tmp.name)
        self.io = FakeIO()
        self.manager = TurretManager(self.path, io=self.io)
        self.io.external["iocontrol.0.toolchanger-fault"] = 0
        self.io.external["iocontrol.0.toolchanger-reason"] = 0

    # ------------------------------------------------------------------
    def test_pins_created(self):
        for name in ("motor", "unclamp", "fault", "station", "state",
                     "count-changes", "station-1-changes", "hours"):
            self.assertIn(name, self.io.pins)
        # fixed ranges so a live station/code change does not miss a pin
        self.assertIn("station-24-changes", self.io.pins)
        self.assertIn("mark-service-32", self.io.pins)

    def test_tool_change_handshake(self):
        m = self.manager
        machine = SimMachine(self.io, start=2)
        self.io.external["iocontrol.0.tool-prep-pocket"] = 3
        self.io.external["iocontrol.0.tool-prepare"] = 1

        def until(mgr, mach):
            return (mgr.fsm.state == State.IDLE and mgr.fsm.changes == 1
                    and mach.t > 1.0)

        self.assertTrue(run_until(m, machine, until), "no completo el cambio")
        self.assertEqual(m.fsm.station, 3)
        self.assertEqual(self.io.own("prepared"), 1)
        self.assertEqual(self.io.external.get("iocontrol.0.tool-prepared"), 1)
        # M6 handshake completes immediately once prepared
        self.io.external["iocontrol.0.tool-change"] = 1
        m._tick(machine.t)
        machine.step()
        self.assertEqual(self.io.own("changed"), 1)
        self.assertEqual(self.io.external.get("iocontrol.0.tool-changed"), 1)

    def test_fault_reports_to_iocontrol(self):
        m = self.manager
        m.fsm.homed = True
        m.fsm.station = 1
        machine = SimMachine(self.io, start=1, pulse_dt=1000.0)
        self.io.external["iocontrol.0.tool-prep-pocket"] = 3
        self.io.external["iocontrol.0.tool-prepare"] = 1
        self.assertTrue(run_until(m, machine, lambda mgr, mach: mgr.fsm.fault))
        self.assertEqual(m.fsm.error, int(Alarm.E_ROTATE_TIMEOUT))
        self.assertEqual(self.io.external.get("iocontrol.0.toolchanger-fault"), 1)
        self.assertEqual(self.io.external.get("iocontrol.0.toolchanger-reason"),
                         int(Alarm.E_ROTATE_TIMEOUT))
        self.assertEqual(len(m.history.to_list()), 1)

    def test_reset_clears_fault(self):
        m = self.manager
        m.fsm.homed = True
        m.fsm.station = 1
        machine = SimMachine(self.io, start=1, pulse_dt=1000.0)
        self.io.external["iocontrol.0.tool-prep-pocket"] = 3
        self.io.external["iocontrol.0.tool-prepare"] = 1
        run_until(m, machine, lambda mgr, mach: mgr.fsm.fault)
        self.io.own_set("reset", 1)
        m._tick(machine.t)
        self.io.own_set("reset", 0)
        machine.pulse_dt = 0.1
        self.assertFalse(m.fsm.fault)
        self.assertFalse(m.fsm.homed)

    def test_cancel_command(self):
        m = self.manager
        machine = SimMachine(self.io, start=2)
        self.io.own_set("cancel", 1)
        m._tick(machine.t)
        self.io.own_set("cancel", 0)
        self.assertTrue(m.fsm.fault)
        self.assertEqual(m.fsm.error, int(Alarm.E_OPERATOR_ABORT))
        self.assertEqual(self.io.external.get("iocontrol.0.toolchanger-fault"), 1)

    def test_lead_param_applied_on_change(self):
        m = self.manager
        m._apply_live_params()
        self.assertEqual(m.fsm.lead, 1)          # from the JSON config
        self.io.param_set("lead-pulses", 2)
        m._apply_live_params()
        self.assertEqual(m.fsm.lead, 2)
        # adaptation owns the lead again until the param changes
        m.fsm.lead = 1
        m._apply_live_params()
        self.assertEqual(m.fsm.lead, 1)

    def test_startup_forgets_reference(self):
        state_path = os.path.join(self.tmp.name, "turret_state.json")
        with open(state_path, "w", encoding="utf-8") as fp:
            json.dump({"fsm": {"station": 5, "homed": True}}, fp)
        io = FakeIO()
        m = TurretManager(self.path, io=io)
        self.assertFalse(m.fsm.homed, "no debe confiar en el homing previo")
        self.assertEqual(m.fsm.station, 5)

    def test_jog_pin(self):
        m = self.manager
        machine = SimMachine(self.io, start=2)
        self.io.own_set("test-enable", 1)
        self.io.own_set("jog", 1)
        self.assertTrue(run_until(m, machine,
                                  lambda mgr, mach: mgr.fsm.state == State.JOG))
        self.assertEqual(self.io.own("motor"), 1)
        self.io.own_set("jog", 0)
        self.assertTrue(run_until(m, machine,
                                  lambda mgr, mach: mgr.fsm.state == State.IDLE))
        self.assertEqual(self.io.own("motor"), 0)

    def test_service_release_and_lock(self):
        m = self.manager
        machine = SimMachine(self.io, start=2)
        self.io.own_set("test-enable", 1)
        self.io.own_set("release", 1)
        m._tick(machine.t)
        self.io.own_set("release", 0)
        self.assertTrue(run_until(m, machine,
                                  lambda mgr, mach: mgr.fsm.state == State.RELEASE))
        self.assertEqual(self.io.own("unclamp"), 1)
        self.assertEqual(self.io.own("motor"), 0)
        self.io.own_set("lock", 1)
        m._tick(machine.t)
        self.io.own_set("lock", 0)
        self.assertTrue(run_until(m, machine,
                                  lambda mgr, mach: mgr.fsm.state == State.IDLE))
        self.assertEqual(self.io.own("unclamp"), 0)

    def test_reference_command(self):
        m = self.manager
        machine = SimMachine(self.io, start=5)
        self.io.own_set("test-enable", 1)
        self.io.own_set("home", 1)
        m._tick(machine.t)
        self.io.own_set("home", 0)
        self.assertTrue(run_until(m, machine,
                                  lambda mgr, mach: mgr.fsm.state == State.IDLE
                                  and mgr.fsm.homed))
        self.assertEqual(m.fsm.station, 1)

    def test_config_reload_timing(self):
        m = self.manager
        cfg = TurretConfig.load(self.path)
        cfg.timing.settle_time = 0.7
        cfg.save(self.path)
        os.utime(self.path, (0, 0))     # force a new mtime
        m._tick(0.0)
        self.assertAlmostEqual(m.fsm.timing.settle_time, 0.7)

    def test_config_reload_stations(self):
        m = self.manager
        cfg = TurretConfig.load(self.path)
        cfg.stations = 12
        cfg.save(self.path)
        os.utime(self.path, (0, 0))
        m._tick(0.0)
        self.assertEqual(m.fsm.stations, 12)
        self.assertFalse(m.fsm.homed)

    def test_maintenance_due(self):
        m = self.manager
        m.fsm.changes = 2
        m._maint_check()
        self.assertEqual(self.io.own("maint-due"), 1)
        self.assertEqual(self.io.own("maint-code"), 1)
        kinds = [e["kind"] for e in m.history.to_list()]
        self.assertIn("maintenance", kinds)

    def test_live_params(self):
        m = self.manager
        self.io.param_set("settle-time", 0.9)
        m._apply_live_params()
        self.assertAlmostEqual(m.cfg.timing.settle_time, 0.9)


if __name__ == "__main__":
    unittest.main(verbosity=2)
