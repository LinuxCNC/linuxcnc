#!/usr/bin/env python3
"""Unit tests for the turret configuration model."""

import os
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "lib", "python"))

from turret.config import Interval, TurretConfig


def valid_config():
    cfg = TurretConfig()
    cfg.pins = {
        "pos1": "hm2.0.input-00",
        "strobe": "hm2.0.input-01",
        "changepos": "hm2.0.input-02",
        "clamped": "hm2.0.input-03",
        "motor": "hm2.0.output-00",
        "unclamp": "hm2.0.output-01",
    }
    cfg.maintenance = [Interval(code=1, name="Pinza", every_changes=1000)]
    return cfg


class TestConfig(unittest.TestCase):

    def test_valid_config(self):
        self.assertEqual(valid_config().validate(), [])

    def test_missing_pins(self):
        cfg = valid_config()
        cfg.pins["strobe"] = ""
        errors = cfg.validate()
        self.assertTrue(any("strobe" in e for e in errors), errors)

    def test_bad_stations_and_mode(self):
        cfg = valid_config()
        cfg.stations = 0
        cfg.mode = "otro"
        errors = cfg.validate()
        self.assertTrue(any("stations" in e for e in errors))
        self.assertTrue(any("modo" in e for e in errors))

    def test_bad_timing(self):
        cfg = valid_config()
        cfg.timing.rotate_timeout = 0
        self.assertTrue(any("rotate_timeout" in e for e in cfg.validate()))

    def test_duplicate_maintenance(self):
        cfg = valid_config()
        cfg.maintenance.append(Interval(code=1, name="dup", every_changes=10))
        self.assertTrue(any("duplicado" in e for e in cfg.validate()))

    def test_interval_without_period(self):
        cfg = valid_config()
        cfg.maintenance.append(Interval(code=9, name="sin intervalo"))
        self.assertTrue(any("sin intervalo" in e for e in cfg.validate()))

    def test_save_load_roundtrip(self):
        cfg = valid_config()
        cfg.stations = 10
        cfg.timing.lead_pulses = 1
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "turret.json")
            cfg.save(path)
            loaded = TurretConfig.load(path)
        self.assertEqual(loaded.stations, 10)
        self.assertEqual(loaded.pins["pos1"], "hm2.0.input-00")
        self.assertEqual(loaded.timing.lead_pulses, 1)
        self.assertEqual(len(loaded.maintenance), 1)
        self.assertEqual(loaded.maintenance[0].every_changes, 1000)


if __name__ == "__main__":
    unittest.main(verbosity=2)
