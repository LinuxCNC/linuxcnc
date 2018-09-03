import pytest
import sys
import os

import_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../../lib/python/')
sys.path.insert(0, os.path.abspath(import_path))

from mklauncher import Mklauncher, LauncherImportance


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
command = python2 run.py
variant = default
'''
    ini = tmpdir.join('launcher.ini')
    ini.write(data)
    return [str(tmpdir)]


@pytest.fixture
def config_dir(tmpdir):
    return str(tmpdir.join('config'))


def test_reading_single_launcher_file_works(context, single_launcher_file):
    launcher = Mklauncher(context, launcher_dirs=single_launcher_file)

    launchers = launcher.container.launcher
    assert len(launchers) == 1
    assert launchers[0].name == 'Demo config'
    assert launchers[0].description == 'My super demo'
    assert launchers[0].command == 'python2 run.py'
    assert launchers[0].info.variant == 'default'


@pytest.fixture
def valid_importance_file(tmpdir):
    data = '''
[/foo/bar/baz]
myconfig = 10
anotherconfig = 2
'''
    ini = tmpdir.join('importances.ini')
    ini.write(data)
    return str(ini)


def test_reading_launcher_importances_works(valid_importance_file):
    importances = LauncherImportance(valid_importance_file)

    importances.load()

    assert importances['/foo/bar/baz:myconfig'] == 10
    assert importances['/foo/bar/baz:anotherconfig'] == 2
    assert importances['/foo/bar/baz:Myconfig'] == 10
    assert importances['/foo/bar/baz:AnotherConfig'] == 2


def test_writing_launcher_importances_works(tmpdir):
    save_file = tmpdir.join('test/output.ini')
    importances = LauncherImportance(str(save_file))

    importances['/my/config/path/:config1'] = 10
    importances['/my/config/path/:config2'] = 2
    importances['/home/alexander/:another_config'] = 0
    importances.save()

    assert os.path.exists(str(save_file))
    data = save_file.read()
    assert '[/my/config/path/]' in data
    assert '[/home/alexander/]' in data
    assert 'config1 = 10' in data
    assert 'config2 = 2' in data
    assert 'another_config = 0' in data


def test_rewriting_launcher_importances_works(valid_importance_file):
    importances = LauncherImportance(valid_importance_file)

    importances.load()
    importances['/foo/bar/baz:myconfig'] = 8
    importances.save()

    assert os.path.exists(valid_importance_file)
    with open(valid_importance_file) as save_file:
        data = save_file.read()
        assert '[/foo/bar/baz]' in data
        assert 'myconfig = 8' in data


def test_regression_paths_with_dot_cause_problems(tmpdir):
    save_file = tmpdir.join('test/output.ini')
    importances = LauncherImportance(str(save_file))

    importances['./foo/bar/:config1'] = 10
    importances['.:config1'] = 2
    importances.save()
    importances.load()
    importances['./foo/bar/:config1'] == 3
    importances.save()

    assert os.path.exists(str(save_file))
    data = save_file.read()
    assert '[./foo/bar/]' in data
    assert 'config1 = 3' not in data  # ConfigParser causes problems with . in the section name


def test_reading_launcher_importances_with_non_existing_file_does_not_throw_error(tmpdir):
    save_file = tmpdir.join('save.ini')
    importances = LauncherImportance(str(save_file))

    importances.load()


def test_updating_launcher_importance_works(context, single_launcher_file, config_dir):
    launcher = Mklauncher(context, launcher_dirs=single_launcher_file, config_dir=config_dir)

    from machinetalk.protobuf.config_pb2 import Launcher
    msg = Launcher()
    msg.index = 0
    msg.importance = 5

    launcher._update_importance(msg)
    launcher._update_launcher_status()

    launchers = launcher.container.launcher
    assert len(launchers) == 1
    assert launchers[0].importance == 5
