# coding=utf-8
import os
import shlex
import sys
from time import *
import subprocess
import signal
from machinekit import compat

_processes = []
_realtime_started = False
_exiting = False


# ends a running Machinekit session
def end_session():
    stop_processes()
    if _realtime_started:  # Stop realtime only when explicitly started
        stop_realtime()


# checks wheter a single command is available or not
def check_command(command):
    process = subprocess.Popen('which ' + command, stdout=subprocess.PIPE, shell=True)
    process.wait()
    if process.returncode != 0:
        print(command + ' not found, check Machinekit installation')
        sys.exit(1)


# checks the whole Machinekit installation
def check_installation():
    commands = ['realtime', 'configserver', 'halcmd', 'haltalk']
    for command in commands:
        check_command(command)


# checks for a running session and cleans it up if necessary
def cleanup_session():
    pids = []
    commands = ['configserver', 'halcmd', 'haltalk', 'webtalk', 'rtapi']
    process = subprocess.Popen(['ps', '-A'], stdout=subprocess.PIPE)
    out, _ = process.communicate()
    for line in out.splitlines():
        for command in commands:
            if command in line:
                pid = int(line.split(None, 1)[0])
                pids.append(pid)

    if any(pids):
        stop_realtime()
        sys.stdout.write("cleaning up leftover session... ")
        sys.stdout.flush()
        for pid in pids:
            try:
                os.killpg(pid, signal.SIGTERM)
            except OSError:
                pass
        sys.stdout.write('done\n')


# starts a command, waits for termination and checks the output
def check_process(command):
    sys.stdout.write("running " + command.split(None, 1)[0] + "... ")
    sys.stdout.flush()
    subprocess.check_call(command, shell=True)
    sys.stdout.write('done\n')


# starts and registers a process
def start_process(command, check=True, wait=1.0):
    sys.stdout.write("starting " + command.split(None, 1)[0] + "... ")
    sys.stdout.flush()
    process = subprocess.Popen(command, shell=True, preexec_fn=os.setsid)
    process.command = command
    if check:
        sleep(wait)
        process.poll()
        if process.returncode is not None:
            raise subprocess.CalledProcessError(process.returncode, command, None)
    _processes.append(process)
    sys.stdout.write('done\n')


# stops a registered process by its name
def stop_process(command):
    for process in _processes:
        process_command = process.command.split(None, 1)[0]
        if command == process_command:
            sys.stdout.write('stopping ' + command + '... ')
            sys.stdout.flush()
            os.killpg(process.pid, signal.SIGTERM)
            process.wait()
            sys.stdout.write('done\n')


# stops all registered processes
def stop_processes():
    for process in _processes:
        sys.stdout.write('stopping ' + process.command.split(None, 1)[0] + '... ')
        sys.stdout.flush()
        os.killpg(process.pid, signal.SIGTERM)
        process.wait()
        sys.stdout.write('done\n')


# loads a HAL configuration file
def load_hal_file(filename, ini=None):
    sys.stdout.write("loading " + filename + '... ')
    sys.stdout.flush()

    _, ext = os.path.splitext(filename)
    if ext == '.py':
        from machinekit import rtapi

        if not rtapi.__rtapicmd:
            rtapi.init_RTAPI()
        if ini is not None:
            from machinekit import config

            config.load_ini(ini)
        globals_ = {}
        with open(filename, 'r') as f:
            exec(f.read(), globals_)
    else:
        command = 'halcmd'
        if ini is not None:
            command += ' -i ' + ini
        command += ' -f ' + filename
        subprocess.check_call(command, shell=True)
    sys.stdout.write('done\n')


# loads a BBIO configuration file
def load_bbio_file(filename):
    check_command('config-pin')
    sys.stdout.write("loading " + filename + '... ')
    sys.stdout.flush()
    subprocess.check_call('config-pin -f ' + filename, shell=True)
    sys.stdout.write('done\n')


# installs a comp RT component
def install_comp(filename):
    install = True
    base, ext = os.path.splitext(os.path.basename(filename))
    flavor = compat.default_flavor()
    module_dir = compat.get_rtapi_config("RTLIB_DIR")
    module_name = flavor.name + '/' + base + flavor.mod_ext
    module_path = os.path.join(module_dir, module_name)
    if os.path.exists(module_path):
        comp_time = os.path.getmtime(filename)
        module_time = os.path.getmtime(module_path)
        if comp_time < module_time:
            install = False

    if install is True:
        if ext == '.icomp':
            cmd_base = 'instcomp'
        else:
            cmd_base = 'comp'
        sys.stdout.write("installing " + filename + '... ')
        sys.stdout.flush()
        if os.access(
            module_dir, os.W_OK
        ):  # if we have write access we might not need sudo
            cmd = '{} --install {}'.format(cmd_base, filename)
        else:
            cmd = 'sudo {} --install {}'.format(cmd_base, filename)

        subprocess.check_call(cmd, shell=True)

        sys.stdout.write('done\n')


# starts realtime
def start_realtime():
    global _realtime_started
    sys.stdout.write("starting realtime...")
    sys.stdout.flush()
    subprocess.check_call('realtime start', shell=True)
    sys.stdout.write('done\n')
    _realtime_started = True


# stops realtime
def stop_realtime():
    global _realtime_started
    sys.stdout.write("stopping realtime... ")
    sys.stdout.flush()
    subprocess.check_call('realtime stop', shell=True)
    sys.stdout.write('done\n')
    _realtime_started = False


# rip the Machinekit environment
def rip_environment(path=None, force=False):
    if force is False and os.getenv('EMC2_PATH') is not None:  # check if already ripped
        return

    if path is None:
        command = None
        script_file_path = os.environ['HOME'] + '/.bashrc'
        if os.path.exists(script_file_path):
            with open(script_file_path) as f:  # use the bashrc
                content = f.readlines()
                for line in content:
                    if 'rip-environment' in line:
                        line = line.strip()
                        if line[0] == '.':
                            command = line

        script_file_path = os.environ['HOME'] + '/machinekit/scripts/rip-environment'
        if os.path.exists(script_file_path):
            command = '. ' + script_file_path

        if command is None:
            sys.stderr.write('Unable to rip environment')
            sys.exit(1)
    else:
        command = '. ' + path + '/scripts/rip-environment'

    process = subprocess.Popen(command + ' && env', stdout=subprocess.PIPE, shell=True)
    for line in process.stdout:
        (key, _, value) = line.partition('=')
        os.environ[key] = value.rstrip()

    sys.path.append(os.environ['PYTHONPATH'])


# checks the running processes and exits when exited
def check_processes():
    for process in _processes:
        process.poll()
        if process.returncode is not None:
            _processes.remove(process)
            end_session()
            if process.returncode != 0:
                sys.exit(1)
            else:
                sys.exit(0)


# register exit signal handlers
def register_exit_handler():
    signal.signal(signal.SIGINT, _exit_handler)
    signal.signal(signal.SIGTERM, _exit_handler)


def _exit_handler(signum, frame):
    del signum  # unused
    del frame  # unused
    global _exiting

    if not _exiting:
        _exiting = True  # prevent double execution
        end_session()
        sys.exit(0)


# set the Machinekit debug level
def set_debug_level(level):
    os.environ['DEBUG'] = str(level)


# set the Machinekit ini
def set_machinekit_ini(ini):
    os.environ['MACHINEKIT_INI'] = ini


def _check_for_non_zombie_process(program):
    try:
        # get process ids
        pids = subprocess.check_output(shlex.split('pgrep {}'.format(program))).strip()
        pids = ' '.join(pids.split('\n'))
        # check each id if it is not zombie
        ps_out = subprocess.check_output(
            shlex.split('ps -p "{}" -o pid=,s='.format(pids))
        ).strip()
        lines = ps_out.split('\n')
        for line in lines:
            pid, status = line.strip().split(' ')
            if status != 'Z':
                return True
    except subprocess.CalledProcessError:
        pass
    return False


# ensure mklauncher is running
def ensure_mklauncher():
    if not _check_for_non_zombie_process('mklauncher'):
        start_process('mklauncher .')
