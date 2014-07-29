#!/usr/bin/env python

from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from unittest import TestCase
import time,os

from machinekit import compat

class TestCompat(TestCase):
    def setUp(self):
        pass

    def test_compat(self):
        # nonexistant kernel module
        assert compat.is_module_loaded("foobarbaz") == False




(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
