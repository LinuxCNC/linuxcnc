#!/usr/bin/python2
# coding=utf-8

import os
import sys
import argparse
from dbus import DBusException

import zmq
import threading
import time
import signal
import math
import subprocess
import fcntl
import shlex
import dbus
import logging
from six.moves import configparser

from operator import attrgetter

from machinekit import service
from machinekit import config

from google.protobuf.message import DecodeError
from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.config_pb2 import (
    Launcher,
    MachineInfo,
    File,
    StdoutLine,
    CLEARTEXT,
)
import machinetalk.protobuf.types_pb2 as pb
from machinetalk.protobuf.object_pb2 import ProtocolParameters


logger = logging.getLogger('mklauncher')


class LauncherImportance(object):
    DEFAULT_IMPORTANCE = 0

    def __init__(self, config_file):
        self._config_file = config_file
        self._importances = {}

    def __setitem__(self, launcher_id, importance):
        launcher_id = launcher_id.lower()
        self._importances[launcher_id] = importance

    def __getitem__(self, launcher_id):
        """ getitem is case insensitive since configparser does not use case sensitive key names """
        launcher_id = launcher_id.lower()
        if launcher_id in self._importances:
            return self._importances[launcher_id]
        else:
            return LauncherImportance.DEFAULT_IMPORTANCE

    def save(self):
        config_dir = os.path.dirname(self._config_file)
        if not os.path.exists(config_dir):
            os.makedirs(config_dir)

        cfg = configparser.ConfigParser()
        for key in self._importances:
            section, name = key.split(':')
            if not cfg.has_section(section):
                cfg.add_section(section)
            cfg.set(section, name, self._importances[key])

        with open(self._config_file, 'w') as config_file:
            cfg.write(config_file)

    def load(self):
        self._importances = {}
        cfg = configparser.ConfigParser()
        cfg.read(self._config_file)

        for section in cfg.sections():
            for (key, value) in cfg.items(section):
                self._importances['%s:%s' % (section, key)] = int(value)

    def __str__(self):
        return str(self._importances)


class Mklauncher(object):
    def __init__(
        self,
        context,
        launcher_dirs=None,
        host='',
        svc_uuid='',
        debug=False,
        name=None,
        host_in_name=True,
        poll_interval=0.5,
        ping_interval=2.0,
        loopback=False,
        config_dir='~/.config/machinekit/mklauncher',
    ):
        if launcher_dirs is None:
            launcher_dirs = []

        self.launcher_dirs = launcher_dirs
        self.host = host
        self.loopback = loopback
        self.name = name
        self.debug = debug
        self.shutdown = threading.Event()
        self.running = False
        self.poll_interval = poll_interval
        self.ping_interval = ping_interval

        # published container
        self.container = Container()
        self.tx_container = Container()
        self.launcher_subscribed = False
        self.launcher_full_update = False
        # command rx and tx containers for reuse
        self.rx = Container()
        self.tx = Container()

        self.processes = {}  # for processes mapped to launcher
        self.terminating = set()  # set of terminating processes

        launchers, ids = self._search_launchers(self.launcher_dirs)
        self._launcher_ids = {}
        for index, launcher in enumerate(launchers):
            self._launcher_ids[index] = ids[launcher.index]
            launcher.index = index
            self.container.launcher.extend([launcher])
            self.tx_container.launcher.add().CopyFrom(launcher)
        logger.debug('parsed launchers:\n%s' % str(self.container))

        config_file = os.path.expanduser(os.path.join(config_dir, 'importances.ini'))
        self._importances = LauncherImportance(config_file)
        self._importances.load()

        # prepare pings
        if self.ping_interval > 0:
            self.ping_ratio = math.floor(self.ping_interval / self.poll_interval)
        else:
            self.ping_ratio = -1
        self.pingCount = 0

        self._create_sockets(context)
        self._create_services(host_in_name, svc_uuid)

    def start(self):
        self._publish_services()
        self._start_threads()

    def stop(self):
        self._terminate_processes()
        self.shutdown.set()

    @staticmethod
    def _search_launchers(directories):
        ini_name = 'launcher.ini'
        config_defaults = {
            'name': 'Launcher',
            'command': '',
            'description': '',
            'image': '',
            'shell': 'false',
            'workdir': '.',
            'type': '',
            'manufacturer': '',
            'model': '',
            'variant': '',
            'priority': '0',
        }

        launchers = []
        ids = {}
        index = 0
        for root_dir in directories:
            for root, _, files in os.walk(root_dir):
                if ini_name not in files:
                    continue

                root = os.path.abspath(os.path.expanduser(root))
                ini_file = os.path.join(root, ini_name)
                cfg = configparser.ConfigParser(config_defaults)
                cfg.read(ini_file)
                for section in cfg.sections():
                    launcher = Launcher()
                    # descriptive data
                    launcher.name = cfg.get(section, 'name')
                    launcher.description = cfg.get(section, 'description')
                    info = MachineInfo()
                    info.type = cfg.get(section, 'type')
                    info.manufacturer = cfg.get(section, 'manufacturer')
                    info.model = cfg.get(section, 'model')
                    info.variant = cfg.get(section, 'variant')
                    launcher.priority = cfg.getint(section, 'priority')
                    launcher.importance = 0
                    launcher.info.MergeFrom(info)
                    # command data
                    launcher.command = cfg.get(section, 'command')
                    launcher.shell = cfg.getboolean(section, 'shell')
                    workdir = cfg.get(section, 'workdir')
                    if not os.path.isabs(workdir):
                        workdir = os.path.join(root, workdir)
                    launcher.workdir = os.path.normpath(workdir)
                    launcher.returncode = 0
                    launcher.running = False
                    launcher.terminating = False
                    # storing the image file
                    image_file = cfg.get(section, 'image')
                    if image_file is not '':
                        if not os.path.isabs(image_file):
                            image_file = os.path.join(root, image_file)
                        file_buffer = open(image_file, 'rb').read()
                        image = File()
                        image.name = os.path.basename(image_file)
                        image.encoding = CLEARTEXT
                        image.blob = file_buffer
                        launcher.image.MergeFrom(image)

                    launcher.index = index
                    index += 1
                    launchers.append(launcher)
                    ids[launcher.index] = '%s:%s' % (root, section)

        # sort using the priority attribute before distribution
        return sorted(launchers, key=attrgetter('priority'), reverse=True), ids

    def _start_threads(self):
        threading.Thread(target=self._process_sockets).start()
        threading.Thread(target=self._poll).start()
        self.running = True

    def _create_sockets(self, context):
        self.context = context
        base_uri = "tcp://"
        if self.loopback:
            base_uri += '127.0.0.1'
        else:
            base_uri += '*'
        self.launcher_socket = context.socket(zmq.XPUB)
        self.launcher_socket.setsockopt(zmq.XPUB_VERBOSE, 1)
        self.launcher_port = self.launcher_socket.bind_to_random_port(base_uri)
        self.launcher_ds_name = self.launcher_socket.get_string(
            zmq.LAST_ENDPOINT, encoding='utf-8'
        )
        self.launcher_ds_name = self.launcher_ds_name.replace('0.0.0.0', self.host)
        self.command_socket = context.socket(zmq.ROUTER)
        self.command_port = self.command_socket.bind_to_random_port(base_uri)
        self.command_ds_name = self.command_socket.get_string(
            zmq.LAST_ENDPOINT, encoding='utf-8'
        )
        self.command_ds_name = self.command_ds_name.replace('0.0.0.0', self.host)

    def _process_sockets(self):
        poll = zmq.Poller()
        poll.register(self.launcher_socket, zmq.POLLIN)
        poll.register(self.command_socket, zmq.POLLIN)

        while not self.shutdown.is_set():
            s = dict(poll.poll(1000))
            if self.launcher_socket in s and s[self.launcher_socket] == zmq.POLLIN:
                self._process_launcher_socket(self.launcher_socket)
            if self.command_socket in s and s[self.command_socket] == zmq.POLLIN:
                self._process_command_socket(self.command_socket)

        self._unpublish_services()
        self.running = False

    def _create_services(self, host_in_name, svc_uuid):
        if self.name is None:
            self.name = 'Machinekit Launcher'
        if host_in_name:
            self.name += ' on ' + self.host
        self.launcher_service = service.Service(
            type_='launcher',
            svc_uuid=svc_uuid,
            dsn=self.launcher_ds_name,
            port=self.launcher_port,
            host=self.host,
            name=self.name,
            loopback=self.loopback,
            debug=self.debug,
        )
        self.commandService = service.Service(
            type_='launchercmd',
            svc_uuid=svc_uuid,
            dsn=self.command_ds_name,
            port=self.command_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )

    def _publish_services(self):
        # Zeroconf
        try:
            self.launcher_service.publish()
            self.commandService.publish()
        except Exception as e:
            logger.error(('cannot register DNS service' + str(e)))
            sys.exit(1)

    def _unpublish_services(self):
        self.launcher_service.unpublish()
        self.commandService.unpublish()

    def _add_pparams_to_message(self):
        parameters = ProtocolParameters()
        parameters.keepalive_timer = int(self.ping_interval * 1000.0)
        self.tx_container.pparams.MergeFrom(parameters)

    def _update_launcher_status(self):
        tx_launcher = Launcher()  # new pb message for tx
        has_update = False

        for launcher in self.container.launcher:
            modified = False
            index = launcher.index

            importance = self._importances[self._launcher_ids[launcher.index]]
            if importance is not launcher.importance:
                tx_launcher.importance = importance
                modified = True

            terminating = False
            if index in self.terminating:
                terminating = True
                self.terminating.remove(index)

            if index in self.processes:
                process = self.processes[index]
                process.poll()
                returncode = process.returncode
                if returncode is None:
                    if not launcher.running:  # update running value
                        if len(launcher.output) > 0:
                            launcher.ClearField(
                                'output'
                            )  # clear output for new processes
                            self.launcher_full_update = True  # request a full update
                        tx_launcher.running = True
                        tx_launcher.returncode = 0
                        modified = True
                    # read stdout
                    stdout_index = len(launcher.output)
                    while True:
                        try:
                            line = process.stdout.read()
                            stdout_line = StdoutLine()
                            stdout_line.index = stdout_index
                            stdout_line.line = line
                            tx_launcher.output.add().MergeFrom(stdout_line)
                            stdout_index += 1
                            modified = True
                        except IOError:  # process has no new line
                            break
                    # send termination status
                    if terminating:
                        tx_launcher.terminating = True
                else:
                    tx_launcher.returncode = returncode
                    tx_launcher.running = False
                    tx_launcher.terminating = False
                    modified = True
                    self.processes.pop(index, None)  # remove from watchlist
            if modified:
                launcher.MergeFrom(tx_launcher)
                tx_launcher.index = index
                self.tx_container.launcher.add().MergeFrom(tx_launcher)
                tx_launcher.Clear()
                has_update = True

        if self.launcher_full_update:
            self._add_pparams_to_message()
            self.tx_container.CopyFrom(self.container)
            self._send_launcher_message(pb.MT_LAUNCHER_FULL_UPDATE)
            self.launcher_full_update = False
        elif has_update:
            self._send_launcher_message(pb.MT_LAUNCHER_INCREMENTAL_UPDATE)

    def _send_launcher_message(self, msg_type):
        logger.debug('sending launcher message')
        self.tx_container.type = msg_type
        tx_buffer = self.tx_container.SerializeToString()
        self.tx_container.Clear()
        self.launcher_socket.send_multipart(['launcher', tx_buffer], zmq.NOBLOCK)

    def _send_command_message(self, identity, msg_type):
        self.tx.type = msg_type
        tx_buffer = self.tx.SerializeToString()
        self.command_socket.send_multipart(identity + [tx_buffer], zmq.NOBLOCK)
        self.tx.Clear()

    def _poll(self):
        while not self.shutdown.is_set():
            if self.launcher_subscribed:
                self._update_launcher_status()
                if self.pingCount == self.ping_ratio:
                    self._send_launcher_message(pb.MT_PING)

            if self.pingCount == self.ping_ratio:
                self.pingCount = 0
            else:
                self.pingCount += 1
            time.sleep(self.poll_interval)

        self.running = False
        return

    def _process_launcher_socket(self, s):
        try:
            rc = s.recv()
            subscription = rc[1:]
            status = rc[0] == "\x01"

            if subscription == 'launcher':
                self.launcher_subscribed = status
                self.launcher_full_update = status

            logger.debug(
                ("process launcher called " + subscription + ' ' + str(status))
            )

        except zmq.ZMQError as e:
            logger.error('ZMQ error: ' + str(e))

    def _start_process(self, index):
        launcher = self.container.launcher[index]
        workdir = launcher.workdir
        shell = launcher.shell
        command = launcher.command
        if shell is False:
            command = shlex.split(command)
        try:
            process = subprocess.Popen(
                command,
                shell=shell,
                cwd=workdir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                stdin=subprocess.PIPE,
                preexec_fn=os.setsid,
            )
        except OSError as e:
            return False, str(e)
        process.command = command
        # set the O_NONBLOCK flag of stdout file descriptor:
        flags = fcntl.fcntl(process.stdout, fcntl.F_GETFL)  # get current stdout flags
        fcntl.fcntl(process.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        self.processes[index] = process
        return True, ''

    def _terminate_process(self, index):
        pid = self.processes[index].pid
        os.killpg(pid, signal.SIGTERM)
        self.terminating.add(index)

    def _kill_process(self, index):
        pid = self.processes[index].pid
        os.killpg(pid, signal.SIGKILL)
        self.terminating.add(index)

    def _terminate_processes(self):
        for index in self.processes.keys():
            self._terminate_process(index)

    def _write_to_stdin_of_process(self, index, data):
        self.processes[index].stdin.write(data)

    @staticmethod
    def _shutdown_system():
        try:
            system_bus = dbus.SystemBus()
            ck_service = system_bus.get_object(
                'org.freedesktop.ConsoleKit', '/org/freedesktop/ConsoleKit/Manager'
            )
            ck_interface = dbus.Interface(
                ck_service, 'org.freedesktop.ConsoleKit.Manager'
            )
            stop_method = ck_interface.get_dbus_method("Stop")
            stop_method()
            return True
        except DBusException:
            return False

    def _update_importance(self, launcher):
        launcher_id = self._launcher_ids[launcher.index]
        self._importances[launcher_id] = launcher.importance
        self._importances.save()

    def _send_command_wrong_params(self, identity, note='wrong parameters'):
        self.tx.note.append(note)
        self._send_command_message(identity, pb.MT_ERROR)

    def _send_command_wrong_index(self, identity):
        self.tx.note.append('wrong index')
        self._send_command_message(identity, pb.MT_ERROR)

    def _process_command_socket(self, s):
        frames = s.recv_multipart()
        identity = frames[:-1]  # multipart id
        message = frames[-1]  # last frame

        logger.debug("process command called, id: %s" % identity)

        try:
            self.rx.ParseFromString(message)
        except DecodeError as e:
            note = 'Protobuf Decode Error: ' + str(e)
            self._send_command_wrong_params(identity, note=note)
            return

        if self.rx.type == pb.MT_PING:
            self._send_command_message(identity, pb.MT_PING_ACKNOWLEDGE)

        elif self.rx.type == pb.MT_LAUNCHER_START:
            if self.rx.HasField('index'):
                index = self.rx.index
                if index >= len(self.container.launcher):
                    self._send_command_wrong_index(identity)
                else:
                    success, note = self._start_process(index)
                    if not success:
                        self.tx.note.append(note)
                        self._send_command_message(identity, pb.MT_ERROR)
            else:
                self._send_command_wrong_params(identity)

        elif self.rx.type == pb.MT_LAUNCHER_TERMINATE:
            if self.rx.HasField('index'):
                index = self.rx.index
                if index >= len(self.container.launcher) or index not in self.processes:
                    self._send_command_wrong_index(identity)
                else:
                    self._terminate_process(index)

        elif self.rx.type == pb.MT_LAUNCHER_KILL:
            if self.rx.HasField('index'):
                index = self.rx.index
                if index >= len(self.container.launcher) or index not in self.processes:
                    self._send_command_wrong_index(identity)
                else:
                    self._kill_process(index)

        elif self.rx.type == pb.MT_LAUNCHER_WRITE_STDIN:
            if self.rx.HasField('index') and self.rx.HasField(
                'name'
            ):  # temporarily using the name field
                index = self.rx.index
                name = self.rx.name
                if index >= len(self.container.launcher) or index not in self.processes:
                    self._send_command_wrong_index(identity)
                else:
                    self._write_to_stdin_of_process(index, name)

        elif self.rx.type == pb.MT_LAUNCHER_CALL:
            self.tx.note.append("process call not allowed")
            self._send_command_message(identity, pb.MT_ERROR)

        elif self.rx.type == pb.MT_LAUNCHER_SHUTDOWN:
            if not self._shutdown_system():
                self.tx.note.append("cannot shutdown system: DBus error")
                self._send_command_message(identity, pb.MT_ERROR)

        elif self.rx.type == pb.MT_LAUNCHER_SET:
            for launcher in self.rx.launcher:
                if not launcher.HasField('index') or not launcher.HasField(
                    'importance'
                ):
                    self._send_command_wrong_params(identity)
                    continue

                index = launcher.index
                if index >= len(self.container.launcher):
                    self._send_command_wrong_index(identity)
                else:
                    self._update_importance(launcher)

        else:
            self.tx.note.append("unknown command")
            self._send_command_message(identity, pb.MT_ERROR)


shutdown = False


def _exit_handler(signum, frame):
    del signum  # ignored
    del frame  # ignored
    global shutdown
    shutdown = True


# register exit signal handlers
def register_exit_handler():
    signal.signal(signal.SIGINT, _exit_handler)
    signal.signal(signal.SIGTERM, _exit_handler)


def check_exit():
    global shutdown
    return shutdown


def main():
    parser = argparse.ArgumentParser(
        description='mklauncher is Machinetalk based session/configuration launcher for Machinekit'
    )
    parser.add_argument(
        '-n', '--name', help='Name of the machine', default="Machinekit Launcher"
    )
    parser.add_argument(
        '-s',
        '--suppress_ip',
        help='Do not show ip of machine in service name',
        action='store_false',
    )
    parser.add_argument('-d', '--debug', help='Enable debug mode', action='store_true')
    parser.add_argument(
        'dirs',
        nargs='*',
        help="List of directories to scan for launcher configurations",
    )

    args = parser.parse_args()
    debug = args.debug

    logging.basicConfig()
    if debug:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    mkconfig = config.Config()
    mkini = os.getenv("MACHINEKIT_INI")
    if mkini is None:
        mkini = mkconfig.MACHINEKIT_INI
    if not os.path.isfile(mkini):
        sys.stderr.write("MACHINEKIT_INI " + mkini + " does not exist\n")
        sys.exit(1)

    mki = configparser.ConfigParser()
    mki.read(mkini)
    uuid = mki.get("MACHINEKIT", "MKUUID")
    remote = mki.getint("MACHINEKIT", "REMOTE")

    if remote == 0:
        logger.info(
            "Remote communication is deactivated, configserver will use the loopback interfaces"
        )
        logger.info(("set REMOTE in " + mkini + " to 1 to enable remote communication"))

    logger.debug("announcing mklauncher")

    context = zmq.Context()
    context.linger = 0

    register_exit_handler()

    hostname = '%(fqdn)s'  # replaced by service announcement
    mklauncher = Mklauncher(
        context,
        svc_uuid=uuid,
        host=hostname,
        launcher_dirs=args.dirs,
        name=args.name,
        host_in_name=bool(args.suppress_ip),
        loopback=(not remote),
        debug=debug,
    )
    mklauncher.start()

    while mklauncher.running and not check_exit():
        time.sleep(1)

    logger.debug('stopping threads')
    mklauncher.stop()

    # wait for all threads to terminate
    while threading.active_count() > 1:
        time.sleep(0.1)

    logger.debug('threads stopped')
    sys.exit(0)


if __name__ == "__main__":
    main()
