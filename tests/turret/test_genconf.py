#!/usr/bin/env python3
"""Unit tests for the F2 configuration generator (turret.json -> .conf)."""

import os
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "lib", "python"))

from turret import genconf
from turret.config import Interval, TurretConfig


def valid_config():
    cfg = TurretConfig()
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
    }
    cfg.maintenance = [Interval(code=1, name="Pinza", every_changes=1000)]
    return cfg


class TestGenConf(unittest.TestCase):

    def test_generate_keys(self):
        text = genconf.generate(valid_config())
        self.assertIn("stations = 8\n", text)
        self.assertIn("pins.pos1 = sim.pos1\n", text)
        self.assertIn("pins.clamp = none\n", text)
        self.assertIn("timing.unclamp_timeout = 2\n", text)
        self.assertIn("logic.require_changepos = 1\n", text)
        self.assertIn("maint.1.name = Pinza\n", text)
        self.assertIn("maint.1.every_changes = 1000\n", text)

    def test_write_and_read_back(self):
        cfg = valid_config()
        with tempfile.TemporaryDirectory() as tmp:
            json_path = os.path.join(tmp, "turret.json")
            cfg.save(json_path)
            conf = genconf.write(json_path)
            self.assertEqual(conf, os.path.join(tmp, "turret.conf"))
            text = open(conf).read()
        self.assertIn("pins.motor = sim.motor", text)
        self.assertIn("maint.1.every_hours = 0", text)

    def test_invalid_config_rejected(self):
        cfg = valid_config()
        cfg.pins["strobe"] = ""
        with tempfile.TemporaryDirectory() as tmp:
            json_path = os.path.join(tmp, "turret.json")
            cfg.save(json_path)
            with self.assertRaises(ValueError):
                genconf.write(json_path)


if __name__ == "__main__":
    unittest.main(verbosity=2)
