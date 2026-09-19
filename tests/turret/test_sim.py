#!/usr/bin/env python3
"""Unit tests for the turret mechanism simulator (fake HAL module)."""

import importlib.util
import os
import sys
import types
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SIM = os.path.join(ROOT, "configs", "sim", "turret", "sim_turret.py")


class FakePin:
    def __init__(self, value=0):
        self.value = value


class FakeComponent:
    def __init__(self, name):
        self.name = name
        self.pins = {}

    def newpin(self, name, kind, direction):
        pin = FakePin(0)
        self.pins[name] = pin
        return pin

    def ready(self):
        pass


class FakeHal(types.ModuleType):
    class Type:
        BIT = "bit"
        S32 = "s32"
        FLOAT = "float"

    class Dir:
        IN = "in"
        OUT = "out"

    def component(self, name):
        return FakeComponent(name)


sys.modules["hal"] = FakeHal("hal")

spec = importlib.util.spec_from_file_location("sim_turret", SIM)
sim_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sim_mod)


class TestSim(unittest.TestCase):

    def test_rotation_pulses_and_coast(self):
        sim = sim_mod.TurretSim(stations=8, pulse_period=0.05, coast_pulses=1,
                                start=2, changepos_delay=0.05)
        sim.comp.ready()
        sim.motor.value = 1
        t = 0.0
        pulses = 0
        prev = sim.station
        for _ in range(40):
            t += 0.01
            sim.tick(t, 0.01)
            if sim.station != prev:
                pulses += 1
                prev = sim.station
        self.assertGreaterEqual(pulses, 3)
        # stop: one coast pulse then seated
        sim.motor.value = 0
        station_at_stop = sim.station
        coast_seen = False
        for _ in range(30):
            t += 0.01
            sim.tick(t, 0.01)
            if sim.station != station_at_stop:
                coast_seen = True
        self.assertTrue(coast_seen, "no hubo pulso de coast")
        self.assertEqual(sim.changepos.value, 1, "no quedo asentada")
        self.assertEqual(sim.pos1.value, 1 if sim.station == 1 else 0)

    def test_clamp_dynamics(self):
        sim = sim_mod.TurretSim(start=2, unclamp_delay=0.05, clamp_delay=0.10)
        sim.comp.ready()
        t = 0.0
        self.assertEqual(sim.clamped.value, 1)
        sim.unclamp.value = 1
        for _ in range(20):
            t += 0.01
            sim.tick(t, 0.01)
        self.assertEqual(sim.clamped.value, 0, "no desbloqueo")
        sim.unclamp.value = 0
        for _ in range(20):
            t += 0.01
            sim.tick(t, 0.01)
        self.assertEqual(sim.clamped.value, 1, "no bloqueo")

    def test_station_pin(self):
        sim = sim_mod.TurretSim(stations=6, start=4)
        sim.comp.ready()
        sim.tick(0.0, 0.01)
        self.assertEqual(sim.station_pin.value, 4)


if __name__ == "__main__":
    unittest.main(verbosity=2)
