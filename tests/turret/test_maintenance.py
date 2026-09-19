#!/usr/bin/env python3
"""Unit tests for the turret maintenance counters and history."""

import os
import sys
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "lib", "python"))

from turret.config import Interval
from turret.maintenance import History, Maintenance


class TestMaintenance(unittest.TestCase):

    def test_due_by_changes(self):
        m = Maintenance([Interval(code=1, name="Pinza", every_changes=100)])
        self.assertEqual(m.due(99, 0), [])
        due = m.due(100, 0)
        self.assertEqual(len(due), 1)
        self.assertEqual(due[0]["code"], 1)

    def test_due_by_hours(self):
        m = Maintenance([Interval(code=3, name="Filtro", every_hours=10.0)])
        self.assertEqual(m.due(0, 9.9), [])
        self.assertEqual(len(m.due(0, 10.0)), 1)

    def test_mark_service_resets_baseline(self):
        m = Maintenance([Interval(code=1, name="Pinza", every_changes=100)])
        m.due(150, 0)
        m.mark_service(1, 150, 0, now=1000.0)
        self.assertEqual(m.due(150, 0), [])
        self.assertEqual(len(m.due(249, 0)), 0)
        self.assertEqual(len(m.due(250, 0)), 1)

    def test_progress(self):
        m = Maintenance([Interval(code=1, name="Pinza", every_changes=100)])
        self.assertAlmostEqual(m.progress(50, 0)[0]["progress"], 0.5)
        self.assertTrue(m.progress(100, 0)[0]["due"])

    def test_persist(self):
        m = Maintenance([Interval(code=1, name="Pinza", every_changes=100)])
        m.mark_service(1, 1234, 5.5, now=1.0)
        other = Maintenance([Interval(code=1, name="Pinza", every_changes=100)])
        other.load_dict(m.to_dict())
        self.assertEqual(other.last_service[1]["changes"], 1234)

    def test_history_cap(self):
        h = History(cap=3)
        for i in range(5):
            h.append(code=i, text="evento %d" % i, now=float(i))
        self.assertEqual(len(h.to_list()), 3)
        self.assertEqual(h.to_list()[0]["code"], 2)

    def test_history_load(self):
        h = History(cap=10)
        h.load_list([{"time": 1.0, "kind": "alarm", "code": 3,
                      "station": 2, "text": "x"}])
        self.assertEqual(h.to_list()[0]["code"], 3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
