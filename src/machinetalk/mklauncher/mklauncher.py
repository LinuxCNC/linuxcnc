#!/usr/bin/env python

import os
import sys
import argparse
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

from operator import attrgetter

from machinekit import service
from machinekit import config

from google.protobuf.message import DecodeError
from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.config_pb2 import Launcher, MachineInfo, File, StdoutLine, CLEARTEXT
import machinetalk.protobuf.types_pb2 as pb
from machinetalk.protobuf.object_pb2 import ProtocolParameters

if sys.version_info >= (3, 0):
    import configparser
else:
    import ConfigParser as configparser

logger = logging.getLogger('mklauncher')


def printError(msg):
    sys.stderr.write('ERROR: ' + msg + '\n')


class LauncherImportance(object):
    DEFAULT_IMPORTANCE = 0

    def __init__(self, config_file):
        self._config_file = config_file
        self._importances = {}

    def __setitem__(self, launcher_id, importance):
        self._importances[launcher_id] = importance

    def __getitem__(self, launcher_id):
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
    def __init__(self, context, launcherDirs=None, host='',
                 svcUuid='', debug=False, name=None, hostInName=True,
                 pollInterval=0.5, pingInterval=2.0, loopback=False,
                 config_dir='~/.config/machinekit/mklauncher'):
        if launcherDirs is None:
            launcherDirs = []

        self.launcherDirs = launcherDirs
        self.host = host
        self.loopback = loopback
        self.name = name
        self.debug = debug
        self.shutdown = threading.Event()
        self.running = False
        self.pollInterval = pollInterval
        self.pingInterval = pingInterval

        # published container
        self.container = Container()
        self.txContainer = Container()
        self.launcherSubscribed = False
        self.launcherFullUpdate = False
        # command rx and tx containers for reuse
        self.rx = Container()
        self.tx = Container()

        self.processes = {}  # for processes mapped to launcher
        self.terminating = set()  # set of terminating processes

        launchers, ids = self._search_launchers(self.launcherDirs)
        self._launcher_ids = {}
        for index, launcher in enumerate(launchers):
            self._launcher_ids[index] = ids[launcher.index]
            launcher.index = index
            self.container.launcher.extend([launcher])
            self.txContainer.launcher.add().CopyFrom(launcher)
        logger.debug('parsed launchers:\n%s' % str(self.container))

        config_file = os.path.expanduser(os.path.join(config_dir, 'importances.ini'))
        self._importances = LauncherImportance(config_file)
        self._importances.load()

        # prepare pings
        if self.pingInterval > 0:
            self.pingRatio = math.floor(self.pingInterval / self.pollInterval)
        else:
            self.pingRatio = -1
        self.pingCount = 0

        self._create_sockets(context)
        self._create_services(hostInName, svcUuid)

    def start(self):
        self._publish_services()
        self._start_threads()

    def stop(self):
        self._terminate_processes()
        self.shutdown.set()

    def _search_launchers(self, directories):
        INI_NAME = 'launcher.ini'
        CONFIG_DEFAULTS = {
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
            'priority': '0'
        }

        launchers = []
        ids = {}
        index = 0
        for rootDir in directories:
            for root, _, files in os.walk(rootDir):
                if INI_NAME not in files:
                    continue

                iniFile = os.path.join(root, INI_NAME)
                cfg = configparser.ConfigParser(CONFIG_DEFAULTS)
                cfg.read(iniFile)
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
                    imageFile = cfg.get(section, 'image')
                    if imageFile is not '':
                        if not os.path.isabs(imageFile):
                            imageFile = os.path.join(root, imageFile)
                        fileBuffer = open(imageFile, 'rb').read()
                        image = File()
                        image.name = os.path.basename(imageFile)
                        image.encoding = CLEARTEXT
                        image.blob = fileBuffer
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
        self.baseUri = "tcp://"
        if self.loopback:
            self.baseUri += '127.0.0.1'
        else:
            self.baseUri += '*'
        self.launcherSocket = context.socket(zmq.XPUB)
        self.launcherSocket.setsockopt(zmq.XPUB_VERBOSE, 1)
        self.launcherPort = self.launcherSocket.bind_to_random_port(self.baseUri)
        self.launcherDsname = self.launcherSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.launcherDsname = self.launcherDsname.replace('0.0.0.0', self.host)
        self.commandSocket = context.socket(zmq.ROUTER)
        self.commandPort = self.commandSocket.bind_to_random_port(self.baseUri)
        self.commandDsname = self.commandSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.commandDsname = self.commandDsname.replace('0.0.0.0', self.host)

    def _process_sockets(self):
        poll = zmq.Poller()
        poll.register(self.launcherSocket, zmq.POLLIN)
        poll.register(self.commandSocket, zmq.POLLIN)

        while not self.shutdown.is_set():
            s = dict(poll.poll(1000))
            if self.launcherSocket in s and s[self.launcherSocket] == zmq.POLLIN:
                self._process_launcher_socket(self.launcherSocket)
            if self.commandSocket in s and s[self.commandSocket] == zmq.POLLIN:
                self._process_command_socket(self.commandSocket)

        self._unpublish_services()
        self.running = False

    def _create_services(self, hostInName, svcUuid):
        if self.name is None:
            self.name = 'Machinekit Launcher'
        if hostInName:
            self.name += ' on ' + self.host
        self.launcherService = \
            service.Service(type='launcher',
                            svcUuid=svcUuid,
                            dsn=self.launcherDsname,
                            port=self.launcherPort,
                            host=self.host,
                            name=self.name,
                            loopback=self.loopback,
                            debug=self.debug)
        self.commandService = \
            service.Service(type='launchercmd',
                            svcUuid=svcUuid,
                            dsn=self.commandDsname,
                            port=self.commandPort,
                            host=self.host,
                            loopback=self.loopback,
                            debug=self.debug)

    def _publish_services(self):
        # Zeroconf
        try:
            self.launcherService.publish()
            self.commandService.publish()
        except Exception as e:
            print(('cannot register DNS service' + str(e)))
            sys.exit(1)

    def _unpublish_services(self):
        self.launcherService.unpublish()
        self.commandService.unpublish()

    def _add_pparams_to_message(self):
        parameters = ProtocolParameters()
        parameters.keepalive_timer = int(self.pingInterval * 1000.0)
        self.txContainer.pparams.MergeFrom(parameters)

    def _update_launcher_status(self):
        txLauncher = Launcher()  # new pb message for tx
        has_update = False

        for launcher in self.container.launcher:
            modified = False
            index = launcher.index

            importance = self._importances[self._launcher_ids[launcher.index]]
            if importance is not launcher.importance:
                txLauncher.importance = importance
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
                            launcher.ClearField('output')  # clear output for new processes
                            self.launcherFullUpdate = True  # request a full update
                        txLauncher.running = True
                        txLauncher.returncode = 0
                        modified = True
                    # read stdout
                    stdoutIndex = len(launcher.output)
                    while True:
                        try:
                            line = process.stdout.readline()
                            stdoutLine = StdoutLine()
                            stdoutLine.index = stdoutIndex
                            stdoutLine.line = line
                            txLauncher.output.add().MergeFrom(stdoutLine)
                            stdoutIndex += 1
                            modified = True
                        except IOError:  # process has no new line
                            break
                    # send termination status
                    if terminating:
                        txLauncher.terminating = True
                else:
                    txLauncher.returncode = returncode
                    txLauncher.running = False
                    txLauncher.terminating = False
                    modified = True
                    self.processes.pop(index, None)  # remove from watchlist
            if modified:
                launcher.MergeFrom(txLauncher)
                txLauncher.index = index
                self.txContainer.launcher.add().MergeFrom(txLauncher)
                txLauncher.Clear()
                has_update = True

        if self.launcherFullUpdate:
            self._add_pparams_to_message()
            self.txContainer.CopyFrom(self.container)
            self._send_launcher_message(pb.MT_LAUNCHER_FULL_UPDATE)
            self.launcherFullUpdate = False
        elif has_update:
            self._send_launcher_message(pb.MT_LAUNCHER_INCREMENTAL_UPDATE)

    def _send_launcher_message(self, msgType):
        logger.debug('sending launcher message')
        self.txContainer.type = msgType
        txBuffer = self.txContainer.SerializeToString()
        self.txContainer.Clear()
        self.launcherSocket.send_multipart(['launcher', txBuffer], zmq.NOBLOCK)

    def _send_command_message(self, identity, msgType):
        self.tx.type = msgType
        txBuffer = self.tx.SerializeToString()
        self.commandSocket.send_multipart(identity + [txBuffer], zmq.NOBLOCK)
        self.tx.Clear()

    def _poll(self):
        while not self.shutdown.is_set():
            if self.launcherSubscribed:
                self._update_launcher_status()
                if (self.pingCount == self.pingRatio):
                    self._send_launcher_message(pb.MT_PING)

            if (self.pingCount == self.pingRatio):
                self.pingCount = 0
            else:
                self.pingCount += 1
            time.sleep(self.pollInterval)

        self.running = False
        return

    def _process_launcher_socket(self, s):
        try:
            rc = s.recv()
            subscription = rc[1:]
            status = (rc[0] == "\x01")

            if subscription == 'launcher':
                self.launcherSubscribed = status
                self.launcherFullUpdate = status

            logger.debug(("process launcher called " + subscription + ' ' + str(status)))

        except zmq.ZMQError as e:
            printError('ZMQ error: ' + str(e))

    def _start_process(self, index):
        launcher = self.container.launcher[index]
        workdir = launcher.workdir
        shell = launcher.shell
        command = launcher.command
        if shell is False:
            command = shlex.split(command)
        try:
            process = subprocess.Popen(command,
                                       shell=shell,
                                       cwd=workdir,
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT,
                                       stdin=subprocess.PIPE,
                                       preexec_fn=os.setsid)
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

    def _shutdown_system(self):
        try:
            systemBus = dbus.SystemBus()
            ckService = systemBus.get_object('org.freedesktop.ConsoleKit',
                                             '/org/freedesktop/ConsoleKit/Manager')
            ckInterface = dbus.Interface(ckService, 'org.freedesktop.ConsoleKit.Manager')
            stopMethod = ckInterface.get_dbus_method("Stop")
            stopMethod()
            return True
        except:
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
                if index >= len(self.container.launcher) \
                   or index not in self.processes:
                    self._send_command_wrong_index(identity)
                else:
                    self._terminate_process(index)

        elif self.rx.type == pb.MT_LAUNCHER_KILL:
            if self.rx.HasField('index'):
                index = self.rx.index
                if index >= len(self.container.launcher) \
                   or index not in self.processes:
                    self._send_command_wrong_index(identity)
                else:
                    self._kill_process(index)

        elif self.rx.type == pb.MT_LAUNCHER_WRITE_STDIN:
            if self.rx.HasField('index') \
               and self.rx.HasField('name'):  # temporarily using the name field
                index = self.rx.index
                name = self.rx.name
                if index >= len(self.container.launcher) \
                   or index not in self.processes:
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
                if not launcher.HasField('index') \
                   or not launcher.HasField('importance'):
                    self._send_command_wrong_params()
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


def _exitHandler(signum, frame):
    del signum  # ignored
    del frame  # ignored
    global shutdown
    shutdown = True


# register exit signal handlers
def register_exit_handler():
    signal.signal(signal.SIGINT, _exitHandler)
    signal.signal(signal.SIGTERM, _exitHandler)


def check_exit():
    global shutdown
    return shutdown


def main():
    parser = argparse.ArgumentParser(description='mklauncher is Machinetalk based session/configuration launcher for Machinekit')
    parser.add_argument('-n', '--name', help='Name of the machine', default="Machinekit Launcher")
    parser.add_argument('-s', '--suppress_ip', help='Do not show ip of machine in service name', action='store_false')
    parser.add_argument('-d', '--debug', help='Enable debug mode', action='store_true')
    parser.add_argument('dirs', nargs='*', help="List of directories to scan for launcher configurations")

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
        print("Remote communication is deactivated, configserver will use the loopback interfaces")
        print(("set REMOTE in " + mkini + " to 1 to enable remote communication"))

    logger.debug("announcing mklauncher")

    context = zmq.Context()
    context.linger = 0

    register_exit_handler()

    hostname = '%(fqdn)s'  # replaced by service announcement
    mklauncher = Mklauncher(context,
                            svcUuid=uuid,
                            host=hostname,
                            launcherDirs=args.dirs,
                            name=args.name,
                            hostInName=bool(args.suppress_ip),
                            loopback=(not remote),
                            debug=debug)
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
