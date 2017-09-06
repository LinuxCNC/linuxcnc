import pytest
import sys
import os

import_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../../lib/python/')
sys.path.insert(0, os.path.abspath(import_path))

from mklauncher import Mklauncher


@pytest.fixture
def context():
    import zmq
    ctx = zmq.Context()
    ctx.linger = 0
    return ctx


@pytest.fixture
def single_launcher_file(tmpdir):
    data = '''
[demo]
name = Demo config
description = My super demo
command = python run.py
variant = default
'''
    ini = tmpdir.join('launcher.ini')
    ini.write(data)
    return [str(tmpdir)]


def test_reading_single_launcher_file_works(context, single_launcher_file):
    launcher = Mklauncher(context, launcherDirs=single_launcher_file)

    launchers = launcher.container.launcher
    assert len(launchers) == 1
    assert launchers[0].name == 'Demo config'
    assert launchers[0].description == 'My super demo'
    assert launchers[0].command == 'python run.py'
    assert launchers[0].info.variant == 'default'
