#!/usr/bin/python2
# coding=utf-8
import os
import sys
from stat import S_ISREG, S_ISDIR
import zmq
import threading
import signal
import time
import argparse
from six.moves import configparser

from machinekit import service
from machinekit import config

from google.protobuf.message import DecodeError
from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.config_pb2 import CLEARTEXT, QT5_QML, GLADEVCP, JAVASCRIPT
import machinetalk.protobuf.types_pb2 as pb


class ConfigServer(object):
    def __init__(
        self,
        context,
        app_dirs=None,
        topdir=".",
        host='',
        svc_uuid=None,
        debug=False,
        name=None,
        host_in_name=True,
        loopback=False,
    ):
        if app_dirs is None:
            app_dirs = []

        self.appDirs = app_dirs
        self.host = host
        self.loopback = loopback
        self.name = name
        self.debug = debug
        self.shutdown = threading.Event()
        self.running = False
        self.cfg = configparser.ConfigParser()

        for rootdir in self.appDirs:
            for root, _, files in os.walk(rootdir):
                if 'description.ini' in files:
                    inifile = os.path.join(root, 'description.ini')
                    cfg = configparser.ConfigParser()
                    cfg.read(inifile)
                    app_name = cfg.get('Default', 'name')
                    description = cfg.get('Default', 'description')
                    app_type = cfg.get('Default', 'type')
                    self.cfg.add_section(app_name)
                    self.cfg.set(app_name, 'description', description)
                    self.cfg.set(app_name, 'type', app_type)
                    self.cfg.set(app_name, 'files', root)
                    if self.debug:
                        print("name: " + cfg.get('Default', 'name'))
                        print("description: " + cfg.get('Default', 'description'))
                        print("type: " + cfg.get('Default', 'type'))
                        print("files: " + root)

        self.rx = Container()
        self.tx = Container()
        self.topdir = topdir
        self.context = context
        self.baseUri = "tcp://"
        if self.loopback:
            self.baseUri += '127.0.0.1'
        else:
            self.baseUri += '*'
        self.socket = context.socket(zmq.ROUTER)
        self.port = self.socket.bind_to_random_port(self.baseUri)
        self.dsname = self.socket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.dsname = self.dsname.replace('0.0.0.0', self.host)

        if self.name is None:
            self.name = "Machinekit"
        if host_in_name:
            self.name += ' on ' + self.host
        self.service = service.Service(
            type_='config',
            svc_uuid=svc_uuid,
            dsn=self.dsname,
            port=self.port,
            host=self.host,
            name=self.name,
            loopback=self.loopback,
            debug=self.debug,
        )

        self.publish()

        threading.Thread(target=self.process_sockets).start()
        self.running = True

    def process_sockets(self):
        poll = zmq.Poller()
        poll.register(self.socket, zmq.POLLIN)

        while not self.shutdown.is_set():
            s = dict(poll.poll(1000))
            if self.socket in s:
                self.process(self.socket)

        self.unpublish()
        self.running = False

    def publish(self):
        try:
            self.service.publish()
        except Exception as e:
            print('cannot register DNS service' + str(e))
            sys.exit(1)

    def unpublish(self):
        self.service.unpublish()

    def stop(self):
        self.shutdown.set()

    @staticmethod
    def type_to_pb(type_):
        if type_ == 'QT5_QML':
            return QT5_QML
        elif type_ == 'GLADEVCP':
            return GLADEVCP
        elif type_ == 'JAVASCRIPT':
            return JAVASCRIPT
        else:
            raise TypeError('Unsupported type %s' % type_)

    def send_msg(self, dest, type_):
        self.tx.type = type_
        tx_buffer = self.tx.SerializeToString()
        if self.debug:
            print("send_msg " + str(self.tx))
        self.tx.Clear()
        self.socket.send_multipart(dest + [tx_buffer], zmq.NOBLOCK)

    def list_apps(self, origin):
        for name in self.cfg.sections():
            app = self.tx.app.add()
            app.name = name
            app.description = self.cfg.get(name, 'description')
            app.type = self.type_to_pb(self.cfg.get(name, 'type'))
        self.send_msg(origin, pb.MT_DESCRIBE_APPLICATION)

    def add_files(self, base_path, path, app):
        if self.debug:
            print("add files " + path)
        for f in os.listdir(path):
            pathname = os.path.join(path, f)
            mode = os.stat(pathname).st_mode
            if S_ISREG(mode):
                filename = os.path.join(os.path.relpath(path, base_path), f)
                if self.debug:
                    print("add " + pathname)
                    print("name " + filename)
                file_buffer = open(pathname, 'rb').read()
                app_file = app.file.add()
                app_file.name = filename
                app_file.encoding = CLEARTEXT
                app_file.blob = file_buffer
            elif S_ISDIR(mode):
                self.add_files(base_path, pathname, app)

    def retrieve_app(self, origin, name):
        if self.debug:
            print("retrieve app " + name)
        app = self.tx.app.add()
        app.name = name
        app.description = self.cfg.get(name, 'description')
        app.type = self.type_to_pb(self.cfg.get(name, 'type'))
        self.add_files(self.cfg.get(name, 'files'), self.cfg.get(name, 'files'), app)

        self.send_msg(origin, pb.MT_APPLICATION_DETAIL)

    def process(self, s):
        frames = s.recv_multipart()
        identity = frames[:-1]  # multipart id
        message = frames[-1]  # last frame

        if self.debug:
            print("process called, id: %s" % identity)

        try:
            self.rx.ParseFromString(message)
        except DecodeError as e:
            note = 'Protobuf Decode Error: ' + str(e)
            self.tx.note.append(note)
            self.send_msg(identity, pb.MT_ERROR)
            return

        if self.rx.type == pb.MT_LIST_APPLICATIONS:
            self.list_apps(identity)

        elif self.rx.type == pb.MT_RETRIEVE_APPLICATION:
            a = self.rx.app[0]
            self.retrieve_app(identity, a.name)

        elif self.rx.type == pb.MT_PING:
            self.send_msg(identity, pb.MT_PING_ACKNOWLEDGE)

        else:
            note = "unsupported request type %d" % self.rx.type
            self.tx.note.append(note)
            self.send_msg(identity, pb.MT_ERROR)


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
        description='Configserver is the entry point for Machinetalk based user interfaces'
    )
    parser.add_argument(
        '-n', '--name', help='Name of the machine', default="Machinekit"
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
        help="List of directories to scan for user interface configurations",
    )

    args = parser.parse_args()

    debug = args.debug

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
        print(
            "Remote communication is deactivated, configserver will use the loopback interfaces"
        )
        print("set REMOTE in " + mkini + " to 1 to enable remote communication")

    if debug:
        print("announcing configserver")

    context = zmq.Context()
    context.linger = 0

    register_exit_handler()

    config_service = None

    try:
        hostname = '%(fqdn)s'  # replaced by service announcement
        config_service = ConfigServer(
            context,
            svc_uuid=uuid,
            topdir=".",
            host=hostname,
            app_dirs=args.dirs,
            name=args.name,
            host_in_name=bool(args.suppress_ip),
            loopback=(not remote),
            debug=debug,
        )

        while config_service.running and not check_exit():
            time.sleep(1)
    except Exception as e:
        print("exception")
        print(e)

    if debug:
        print("stopping threads")
    if config_service is not None:
        config_service.stop()

    # wait for all threads to terminate
    while threading.active_count() > 1:
        time.sleep(0.1)

    if debug:
        print("threads stopped")
    sys.exit(0)


if __name__ == "__main__":
    main()
