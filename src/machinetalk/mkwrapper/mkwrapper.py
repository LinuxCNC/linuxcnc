#!/usr/bin/python2
# -*- coding: UTF-8 -*
import os
import zmq
import threading
import multiprocessing
import time
import math
import socket
import signal
import argparse
import shutil
import tempfile
import subprocess
import re
import codecs
from six.moves import configparser
from six.moves.urllib import parse
import linuxcnc
from machinekit import service, hal
from machinekit import config

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

from google.protobuf.message import DecodeError
from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.types_pb2 import *
from machinetalk.protobuf.status_pb2 import *
from machinetalk.protobuf.preview_pb2 import *
from machinetalk.protobuf.motcmds_pb2 import *
from machinetalk.protobuf.object_pb2 import ProtocolParameters


def print_error(msg):
    sys.stderr.write('ERROR: ' + msg + '\n')


def get_free_port():
    sock = socket.socket()
    sock.bind(('', 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


# noinspection PyClassicStyleClass
class CustomFTPHandler(FTPHandler):
    def on_file_received(self, file_):
        # do something when a file has been received
        pass  # TODO inform client about new file

    def on_incomplete_file_received(self, file_):
        # remove partially uploaded files
        os.remove(file_)


class FileService(threading.Thread):
    def __init__(
        self, ini_file=None, host='', svc_uuid=None, loopback=False, debug=False
    ):
        self.debug = debug
        self.host = host
        self.loopback = loopback
        self.shutdown = threading.Event()
        self.running = False

        # Linuxcnc
        try:
            ini_file = ini_file or os.environ.get('INI_FILE_NAME', '/dev/null')
            self.ini = linuxcnc.ini(ini_file)
            self.directory = self.ini.find('DISPLAY', 'PROGRAM_PREFIX') or os.getcwd()
            self.directory = os.path.expanduser(self.directory)
        except linuxcnc.error as detail:
            print_error(str(detail))
            sys.exit(1)

        self.file_port = get_free_port()
        self.file_dsname = "ftp://" + self.host + ":" + str(self.file_port)

        self.fileService = service.Service(
            type_='file',
            svc_uuid=svc_uuid,
            dsn=self.file_dsname,
            port=self.file_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )

        # FTP
        # Instantiate a dummy authorizer for managing 'virtual' users
        self.authorizer = DummyAuthorizer()

        # anonymous user has full read write access
        self.authorizer.add_anonymous(self.directory, perm="lredwm")

        # Instantiate FTP handler class
        self.handler = CustomFTPHandler
        self.handler.authorizer = self.authorizer

        # Define a customized banner (string returned when client connects)
        self.handler.banner = "welcome to the GCode file service"

        # Instantiate FTP server class and listen on some address
        self.address = ('', self.file_port)
        self.server = FTPServer(self.address, self.handler)

        # set a limit for connections
        self.server.max_cons = 256
        self.server.max_cons_per_ip = 5

        # Zeroconf
        try:
            self.fileService.publish()
        except Exception as e:
            print_error('cannot register DNS service' + str(e))
            sys.exit(1)

        threading.Thread.__init__(self)

    def run(self):
        self.running = True

        # start ftp server
        while not self.shutdown.is_set():
            self.server.serve_forever(timeout=1, blocking=False)

        self.server.close_all()
        self.fileService.unpublish()

        self.running = False
        return

    def stop(self):
        self.shutdown.set()


class PreviewCanonData(object):
    def __init__(self):
        self.tools = []
        self.random_toolchanger = False
        self.parameter_file = ""
        self.axis_mask = 0x0
        self.block_delete = False
        self.angular_units = 1.0
        self.linear_units = 1.0


class PreviewCanon(object):
    def __init__(self, canon, debug=False):
        self.c = canon
        self.debug = debug

    def check_abort(self):
        if self.debug:
            print("check_abort")
        return False

    def change_tool(self, pocket):
        if self.c.random_toolchanger:
            self.c.tools[0], self.c.tools[pocket] = (
                self.c.tools[pocket],
                self.c.tools[0],
            )
        elif pocket == 0:
            self.c.tools[0] = (
                -1,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0,
            )
        else:
            self.c.tools[0] = self.c.tools[pocket]

    def get_tool(self, pocket):
        if 0 <= pocket < len(self.c.tools):
            return self.c.tools[pocket]
        return -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0

    def get_external_angular_units(self):
        return self.c.angular_units or 1.0

    def get_external_length_units(self):
        return self.c.linear_units or 1.0

    def get_axis_mask(self):
        return self.c.axis_mask

    def get_block_delete(self):
        return self.c.block_delete


# Preview class works concurrently using multiprocessing
# Queues are used for communication
class Preview(object):
    def __init__(
        self,
        stat,
        random_toolchanger=False,
        parameter_file="",
        initcode="",
        debug=False,
    ):
        self.debug = debug
        self.filename = ""
        self.unitcode = ""
        self.initcode = initcode
        self.stat = stat
        self.parameter_file = parameter_file
        self.random_toolchanger = random_toolchanger
        self.canon = None
        self.preview = None
        self.error_callback = None

        # multiprocessing tools
        self.bind_event = multiprocessing.Event()
        self.bind_completed_event = multiprocessing.Event()
        self.preview_event = multiprocessing.Event()
        self.preview_completed_event = multiprocessing.Event()
        self.shutdown_event = multiprocessing.Event()
        self.error_event = multiprocessing.Event()
        self.is_started = multiprocessing.Value('b', False, lock=False)
        self.is_bound = multiprocessing.Value('b', False, lock=False)
        self.is_running = multiprocessing.Value('b', False, lock=False)
        self.aborted = multiprocessing.Value('b', False, lock=False)
        self.inqueue = multiprocessing.Queue()  # used to send data to the process
        self.outqueue = multiprocessing.Queue()  # used to get data from the process
        self.process = multiprocessing.Process(target=self.run)
        self.process.start()

    def register_error_callback(self, callback):
        self.error_callback = callback

    def bind(self, preview_uri, status_uri):
        self.inqueue.put((preview_uri, status_uri))
        self.bind_event.set()
        self.bind_completed_event.wait()
        return self.outqueue.get()

    def abort(self):
        self.aborted.value = True

    def program_open(self, filename):
        if os.path.isfile(filename):
            self.filename = filename
        else:
            raise Exception("file does not exist " + filename)

    def start(self):
        if self.is_running.value is True:
            raise Exception("Preview already running")

        # start the monitoring daemon
        thread = threading.Thread(target=self.monitoring_thread)
        thread.daemon = True
        thread.start()

    def stop(self):
        self.shutdown_event.set()
        self.preview_completed_event.set()  # in case we are monitoring
        self.process.join()  # make sure to have one process at exit

    def monitoring_thread(self):
        if not self.is_bound.value:
            raise Exception('Preview is not bound')

        self.is_running.value = True

        # create temp dir
        tempdir = tempfile.mkdtemp()

        # prepare Canon data
        canon = PreviewCanonData()
        tools = []
        for entry in self.stat.tool_table:
            tools.append(tuple(entry))
        canon.tools = tools
        temp_parameter = os.path.join(tempdir, os.path.basename(self.parameter_file))
        if os.path.exists(self.parameter_file):
            shutil.copy(self.parameter_file, temp_parameter)
        canon.parameter_file = temp_parameter
        canon.random_toolchanger = self.random_toolchanger
        canon.axis_mask = self.stat.axis_mask
        canon.block_delete = self.stat.block_delete
        canon.angular_units = self.stat.angular_units
        canon.linear_units = self.stat.linear_units

        self.unitcode = "G%d" % (20 + (self.stat.linear_units == 1))
        # put everything on the process queue
        while not self.inqueue.empty():
            self.inqueue.get_nowait()
        self.inqueue.put((self.filename, self.unitcode, self.initcode, canon))
        # release the dragon
        self.preview_completed_event.clear()
        self.preview_event.set()
        self.preview_completed_event.wait()
        # handle error events
        if self.error_event.wait(0.1):
            (error, line) = self.outqueue.get()
            if self.error_callback is not None:
                self.error_callback(error, line)
            self.error_event.clear()

        # cleanup temp dir
        shutil.rmtree(tempdir)

        self.is_running.value = False

    def run(self):
        import preview  # must be imported in new process to work properly

        self.preview = preview

        self.is_started.value = True

        # waiting for a bind event
        while not self.bind_event.wait(timeout=0.1):
            if (
                self.shutdown_event.is_set()
            ):  # in case someone shuts down when we are not bound
                self.is_started.value = False

        # bind the socket here
        (preview_uri, status_uri) = self.inqueue.get()
        (preview_uri, status_uri) = self.preview.bind(preview_uri, status_uri)
        self.outqueue.put((preview_uri, status_uri))
        self.is_bound.value = True
        self.bind_completed_event.set()
        if self.debug:
            print('Preview socket bound')

        # wait for preview request or shutdown
        # event handshaking is used to synchronize the monitoring thread
        # and the process
        while not self.shutdown_event.is_set():
            if self.preview_event.wait(timeout=0.1):
                self.do_preview()
                self.preview_completed_event.set()
                self.preview_event.clear()

        if self.debug:
            print('Preview process exited')
        self.is_started.value = False

    def do_preview(self):
        # get all stuff that is modified at runtime
        (filename, unitcode, initcode, canon_data) = self.inqueue.get()
        # make abort possible
        canon = PreviewCanon(canon_data, self.debug)
        self.aborted.value = False
        canon.check_abort = lambda: self.aborted.value
        if self.debug:
            print("Preview starting")
            print("Filename: {}".format(filename))
            print("Unitcode: {}".format(unitcode))
            print("Initcode: {}".format(initcode))
        try:
            # here we do all the actual work...
            (result, last_sequence_number) = self.preview.parse(
                filename, canon, unitcode, initcode
            )

            # check if we encountered a error during execution
            if result > self.preview.MIN_ERROR:
                error = " gcode error: %s " % (self.preview.strerror(result))
                line = last_sequence_number - 1
                if self.debug:
                    print_error("preview: " + filename)
                    print_error(error + " on line " + str(line))
                # pass error through queue
                self.outqueue.put((error, str(line)))
                self.error_event.set()

        except Exception as e:
            error = "preview error: " + str(e)
            if self.debug:
                print_error(error)
            self.outqueue.put((error, "0"))
            self.error_event.set()

        if self.debug:
            print("Preview exiting")


class StatusValues(object):
    def __init__(self):
        self.io = EmcStatusIo()
        self.config = EmcStatusConfig()
        self.motion = EmcStatusMotion()
        self.task = EmcStatusTask()
        self.interp = EmcStatusInterp()
        self.ui = EmcStatusUI()

    def clear(self):
        self.io.Clear()
        self.config.Clear()
        self.motion.Clear()
        self.task.Clear()
        self.interp.Clear()
        self.ui.Clear()


class LinuxCNCWrapper(object):
    def __init__(
        self,
        context,
        host='',
        loopback=False,
        ini_file=None,
        svc_uuid=None,
        poll_interval_s=None,
        ping_interval=2,
        debug=False,
    ):
        self.debug = debug
        self.host = host
        self.loopback = loopback
        self.ping_interval = ping_interval
        self.shutdown = threading.Event()
        self.running = False

        # synchronization locks
        self.command_lock = threading.Lock()
        self.status_lock = threading.Lock()
        self.error_lock = threading.Lock()
        self.error_note_lock = threading.Lock()

        # status
        self.status = StatusValues()
        self.status_tx = StatusValues()
        self.motion_subscribed = False
        self.motion_full_update = False
        self.motion_first_run = True
        self.io_subscribed = False
        self.io_full_update = False
        self.io_first_run = True
        self.io_tool_table_count = 0
        self.io_tool_table_loaded = False
        self.task_subscribed = False
        self.task_full_update = False
        self.task_first_run = True
        self.config_subscribed = False
        self.config_full_update = False
        self.config_first_run = True
        self.interp_subscribed = False
        self.interp_full_update = False
        self.interp_first_run = True
        self.ui_subscribed = False
        self.ui_full_update = False
        self.ui_first_run = True
        self.status_service_subscribed = False

        self.text_subscribed = False
        self.display_subscribed = False
        self.error_subscribed = False
        self.error_service_subscribed = False
        self.new_error_subscription = False

        self.linuxcnc_errors = []
        self.program_extensions = {}

        # Linuxcnc
        try:
            self.stat = linuxcnc.stat()
            self.command = linuxcnc.command()
            self.error = linuxcnc.error_channel()

            ini_file = ini_file or os.environ.get('INI_FILE_NAME', '/dev/null')
            self.ini = linuxcnc.ini(ini_file)
            self.directory = self.ini.find('DISPLAY', 'PROGRAM_PREFIX') or os.getcwd()
            self.directory = os.path.abspath(os.path.expanduser(self.directory))
            self.poll_interval_s = float(
                poll_interval_s or self.ini.find('DISPLAY', 'CYCLE_TIME') or 0.1
            )
            self.interp_parameter_file = (
                self.ini.find('RS274NGC', 'PARAMETER_FILE') or "linuxcnc.var"
            )
            self.interp_parameter_file = os.path.abspath(
                os.path.expanduser(self.interp_parameter_file)
            )
            self.interp_init_code = self.ini.find("EMC", "RS274NGC_STARTUP_CODE") or ""
            if self.interp_init_code == "":
                self.interp_init_code = (
                    self.ini.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""
                )
            self.interp_init_code = (
                self.ini.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""
            )
            self.random_tool_changer = (
                self.ini.find("EMCIO", "RANDOM_TOOL_CHANGER") or 0
            )

            # setup program extensions
            extensions = self.ini.findall("FILTER", "PROGRAM_EXTENSION")
            for line in extensions:
                split_line = line.split(' ')
                split_line = split_line[0].split(',')
                for extension in split_line:
                    if extension[0] == '.':
                        extension = extension[1:]
                    program = self.ini.find("FILTER", extension) or ""
                    if program is not "":
                        self.program_extensions[extension] = program

            # initialize total line count
            self.total_lines = 0
            # initialize tool table path
            self.tool_table_path = self.ini.find('EMCIO', 'TOOL_TABLE') or ''
            if self.tool_table_path is not '':
                self.tool_table_path = os.path.abspath(
                    os.path.expanduser(self.tool_table_path)
                )
            # If specified in the ini, try to open the  default file
            open_file = self.ini.find('DISPLAY', 'OPEN_FILE') or ""
            open_file = open_file.strip('"')  # quote signs are allowed
            if open_file != "":
                open_file = os.path.abspath(os.path.expanduser(open_file))
                file_name = os.path.basename(open_file)
                file_path = os.path.join(self.directory, file_name)
                shutil.copy(open_file, file_path)
                if self.debug:
                    print("loading default file {}".format(open_file))
                file_path = self.preprocess_program(file_path)
                self.command.mode(linuxcnc.MODE_AUTO)
                self.command.wait_complete()
                self.command.program_open(file_path)

        except linuxcnc.error as detail:
            print_error(str(detail))
            sys.exit(1)

        if self.ping_interval > 0:
            self.ping_ratio = math.floor(self.ping_interval / self.poll_interval_s)
        else:
            self.ping_ratio = -1
        self.ping_count = 0

        self.rx = Container()  # Used by the command socket
        self.tx_status = Container()  # Status socket - PUB-SUB
        self.tx_command = Container()  # Command socket - ROUTER-DEALER
        self.tx_error = Container()  # Error socket - PUB-SUB
        self.context = context
        self.base_uri = "tcp://"
        if self.loopback:
            self.base_uri += '127.0.0.1'
        else:
            self.base_uri += '*'
        self.status_socket = context.socket(zmq.XPUB)
        self.status_socket.setsockopt(zmq.XPUB_VERBOSE, 1)
        self.status_port = self.status_socket.bind_to_random_port(self.base_uri)
        self.status_dsname = self.status_socket.get_string(
            zmq.LAST_ENDPOINT, encoding='utf-8'
        )
        self.status_dsname = self.status_dsname.replace('0.0.0.0', self.host)
        self.error_socket = context.socket(zmq.XPUB)
        self.error_socket.setsockopt(zmq.XPUB_VERBOSE, 1)
        self.error_port = self.error_socket.bind_to_random_port(self.base_uri)
        self.error_dsname = self.error_socket.get_string(
            zmq.LAST_ENDPOINT, encoding='utf-8'
        )
        self.error_dsname = self.error_dsname.replace('0.0.0.0', self.host)
        self.command_socket = context.socket(zmq.ROUTER)
        self.command_port = self.command_socket.bind_to_random_port(self.base_uri)
        self.command_dsname = self.command_socket.get_string(
            zmq.LAST_ENDPOINT, encoding='utf-8'
        )
        self.command_dsname = self.command_dsname.replace('0.0.0.0', self.host)
        self.preview = Preview(
            stat=self.stat,
            random_toolchanger=self.random_tool_changer,
            parameter_file=self.interp_parameter_file,
            initcode=self.interp_init_code,
            debug=self.debug,
        )
        (self.preview_dsname, self.previewstatus_dsname) = self.preview.bind(
            self.base_uri + ':*', self.base_uri + ':*'
        )
        self.preview_dsname = self.preview_dsname.replace('0.0.0.0', self.host)
        self.previewstatus_dsname = self.previewstatus_dsname.replace(
            '0.0.0.0', self.host
        )
        self.preview.register_error_callback(self.preview_error)
        self.preview_port = parse.urlparse(self.preview_dsname).port
        self.previewstatus_port = parse.urlparse(self.previewstatus_dsname).port

        self.status_service = service.Service(
            type_='status',
            svc_uuid=svc_uuid,
            dsn=self.status_dsname,
            port=self.status_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )
        self.error_service = service.Service(
            type_='error',
            svc_uuid=svc_uuid,
            dsn=self.error_dsname,
            port=self.error_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )
        self.command_service = service.Service(
            type_='command',
            svc_uuid=svc_uuid,
            dsn=self.command_dsname,
            port=self.command_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )
        self.preview_service = service.Service(
            type_='preview',
            svc_uuid=svc_uuid,
            dsn=self.preview_dsname,
            port=self.preview_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )
        self.previewstatus_service = service.Service(
            type_='previewstatus',
            svc_uuid=svc_uuid,
            dsn=self.previewstatus_dsname,
            port=self.previewstatus_port,
            host=self.host,
            loopback=self.loopback,
            debug=self.debug,
        )

        self.publish()

        threading.Thread(target=self.process_sockets, name="process_sockets").start()
        self.running = True

    def process_sockets(self):
        poll = zmq.Poller()
        poll.register(self.status_socket, zmq.POLLIN)
        poll.register(self.error_socket, zmq.POLLIN)
        poll.register(self.command_socket, zmq.POLLIN)

        next_poll = time.time() + self.poll_interval_s
        poll_delay = self.poll_interval_s * 1000  # convert to ms
        while not self.shutdown.is_set():
            s = dict(poll.poll(poll_delay))
            if self.status_socket in s and s[self.status_socket] == zmq.POLLIN:
                self.process_status(self.status_socket)
            if self.error_socket in s and s[self.error_socket] == zmq.POLLIN:
                self.process_error(self.error_socket)
            if self.command_socket in s and s[self.command_socket] == zmq.POLLIN:
                self.process_command(self.command_socket)

            poll_delay = (next_poll - time.time()) * 1000  # convert to ms
            if poll_delay > 0:
                continue

            next_poll = time.time() + self.poll_interval_s
            poll_delay = self.poll_interval_s * 1000  # convert to ms

            try:
                if self.status_service_subscribed:
                    self.stat.poll()
                    self.update_status(self.stat)
                    if self.ping_count == self.ping_ratio:
                        self.ping_status()
                if self.error_service_subscribed:
                    error = self.error.poll()
                    self.update_error(error)
                    if self.ping_count == self.ping_ratio:
                        self.ping_error()
            except linuxcnc.error as detail:
                print_error(str(detail))
                self.stop()

            if self.ping_count == self.ping_ratio:
                self.ping_count = 0
            else:
                self.ping_count += 1

        self.unpublish()
        self.running = False
        return

    def publish(self):
        # Zeroconf
        try:
            self.status_service.publish()
            self.error_service.publish()
            self.command_service.publish()
            self.preview_service.publish()
            self.previewstatus_service.publish()
        except Exception as e:
            print_error('cannot register DNS service' + str(e))
            sys.exit(1)

    def unpublish(self):
        self.status_service.unpublish()
        self.error_service.unpublish()
        self.command_service.unpublish()
        self.preview_service.unpublish()
        self.previewstatus_service.unpublish()

    def stop(self):
        self.shutdown.set()
        self.preview.stop()

    # handle program extensions
    def preprocess_program(self, file_path):
        file_name, extension = os.path.splitext(file_path)
        extension = extension[1:]  # remove dot
        if extension in self.program_extensions:
            program = self.program_extensions[extension]
            new_file_name = file_name + '.ngc'
            try:
                out_file = open(new_file_name, 'w')
                process = subprocess.Popen([program, file_path], stdout=out_file)
                # subprocess.check_output([program, filePath],
                unused_out, err = process.communicate()
                retcode = process.poll()
                if retcode:
                    raise subprocess.CalledProcessError(retcode, '', output=err)
                out_file.close()
                file_path = new_file_name
            except IOError as e:
                self.add_error(str(e))
                return ''
            except subprocess.CalledProcessError as e:
                self.add_error('%s failed: %s' % (program, str(e)))
                return ''
        # get number of lines
        with open(file_path) as f:
            self.total_lines = sum(1 for _ in f)
        return file_path

    def load_tool_table(self, io, tx_io):
        if self.tool_table_path is '':
            return

        with codecs.open(self.tool_table_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # parsing pocket number and comment, not emc status object
        tool_map = {}
        regex = re.compile(r'(?:.*?T(\d+))(?:.*?P(\d+))?(?:.*;(.*))?', re.IGNORECASE)
        for line in lines:
            match = regex.match(line)
            if match:
                id_ = int(match.group(1))
                pocket = match.group(2)
                comment = match.group(3)
                if pocket == '':
                    pocket = 0
                tool_map[id_] = {'pocket': int(pocket), 'comment': comment}

        for i, tool_result in enumerate(io.tool_table):
            tx_tool_result = None
            for result in tx_io.tool_table:
                if result.index == tool_result.index:
                    tx_tool_result = result
            if not tx_tool_result:
                tx_tool_result = tx_io.tool_table.add()
                tx_tool_result.CopyFrom(tool_result)
            id_ = tool_result.id
            if id_ in tool_map:
                tool_result.pocket = tool_map[id_]['pocket'] or 0
                tool_result.comment = tool_map[id_]['comment'] or ''
                tx_tool_result.pocket = tool_map[id_]['pocket'] or 0
                tx_tool_result.comment = tool_map[id_]['comment'] or ''

    def update_tool_table(self, tool_table):
        if self.tool_table_path is '':
            return False

        with codecs.open(self.tool_table_path, 'w', encoding='utf-8') as f:
            for tool in tool_table:
                line = (
                    'T%d P%d D%f X%+f Y%+f Z%+f A%+f B%+f C%+f U%+f V%+f W%+f I%+f J%+f Q%d ;%s\n'
                    % (
                        tool.id,
                        tool.pocket,
                        tool.diameter,
                        tool.offset.x,
                        tool.offset.y,
                        tool.offset.z,
                        tool.offset.a,
                        tool.offset.b,
                        tool.offset.c,
                        tool.offset.u,
                        tool.offset.v,
                        tool.offset.w,
                        tool.frontangle,
                        tool.backangle,
                        tool.orientation,
                        tool.comment,
                    )
                )
                f.write(line)

        return True

    @staticmethod
    def not_equal(a, b):
        threshold = 0.0001
        return abs(a - b) > threshold

    @staticmethod
    def zero_position():
        position = Position()
        position.x = 0.0
        position.y = 0.0
        position.z = 0.0
        position.a = 0.0
        position.b = 0.0
        position.c = 0.0
        position.u = 0.0
        position.v = 0.0
        position.w = 0.0
        return position

    @staticmethod
    def check_position(old_position, new_position):
        modified = False
        tx_position = Position()

        if LinuxCNCWrapper.not_equal(old_position.x, new_position[0]):
            tx_position.x = new_position[0]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.y, new_position[1]):
            tx_position.y = new_position[1]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.z, new_position[2]):
            tx_position.z = new_position[2]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.a, new_position[3]):
            tx_position.a = new_position[3]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.b, new_position[4]):
            tx_position.b = new_position[4]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.c, new_position[5]):
            tx_position.c = new_position[5]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.u, new_position[6]):
            tx_position.u = new_position[6]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.v, new_position[7]):
            tx_position.v = new_position[7]
            modified = True
        if LinuxCNCWrapper.not_equal(old_position.w, new_position[8]):
            tx_position.w = new_position[8]
            modified = True

        if modified:
            return True, tx_position
        else:
            del tx_position
            return False, None

    @staticmethod
    def update_proto_value(obj, tx_obj, prop, value):
        if getattr(obj, prop) != value:
            setattr(obj, prop, value)
            setattr(tx_obj, prop, value)
            return True
        return False

    @staticmethod
    def update_proto_float(obj, tx_obj, prop, value):
        if LinuxCNCWrapper.not_equal(getattr(obj, prop), value):
            setattr(obj, prop, value)
            setattr(tx_obj, prop, value)
            return True
        return False

    @staticmethod
    def update_proto_list(obj, tx_obj, tx_obj_item, prop, values, default):
        modified = False
        for index, value in enumerate(values):
            tx_obj_item.Clear()
            obj_modified = False

            if len(obj) == index:
                obj.add()
                obj[index].index = index
                setattr(obj[index], prop, default)

            obj_item = obj[index]
            obj_modified |= LinuxCNCWrapper.update_proto_value(
                obj_item, tx_obj_item, prop, value
            )

            if obj_modified:
                tx_obj_item.index = index
                tx_obj.add().CopyFrom(tx_obj_item)
                modified = True

        return modified

    @staticmethod
    def update_proto_position(obj, tx_obj, prop, value):
        modified, tx_position = LinuxCNCWrapper.check_position(
            getattr(obj, prop), value
        )
        if modified:
            getattr(obj, prop).MergeFrom(tx_position)
            getattr(tx_obj, prop).CopyFrom(tx_position)

        del tx_position
        return modified

    def update_config_value(self, prop, value):
        return self.update_proto_value(
            self.status.config, self.status_tx.config, prop, value
        )

    def update_config_float(self, prop, value):
        return self.update_proto_float(
            self.status.config, self.status_tx.config, prop, value
        )

    def update_io_value(self, prop, value):
        return self.update_proto_value(self.status.io, self.status_tx.io, prop, value)

    def update_task_value(self, prop, value):
        return self.update_proto_value(
            self.status.task, self.status_tx.task, prop, value
        )

    def update_interp_value(self, prop, value):
        return self.update_proto_value(
            self.status.interp, self.status_tx.interp, prop, value
        )

    def update_motion_value(self, prop, value):
        return self.update_proto_value(
            self.status.motion, self.status_tx.motion, prop, value
        )

    def update_motion_float(self, prop, value):
        return self.update_proto_float(
            self.status.motion, self.status_tx.motion, prop, value
        )

    def update_ui_value(self, prop, value):
        return self.update_proto_value(self.status.ui, self.status_tx.ui, prop, value)

    def update_config(self, stat):
        modified = False

        if self.config_first_run:
            self.status.config.default_acceleration = 0.0
            self.status.config.angular_units = ANGULAR_UNITS_DEGREES
            self.status.config.axes = 0
            self.status.config.axis_mask = 0
            self.status.config.cycle_time = 0.0
            self.status.config.debug = 0
            self.status.config.kinematics_type = KINEMATICS_IDENTITY
            self.status.config.linear_units = LINEAR_UNITS_MM
            self.status.config.max_acceleration = 0.0
            self.status.config.max_velocity = 0.0
            self.status.config.default_velocity = 0.0
            self.status.config.position_offset = EMC_CONFIG_RELATIVE_OFFSET
            self.status.config.position_feedback = EMC_CONFIG_ACTUAL_FEEDBACK
            self.status.config.max_feed_override = 0.0
            self.status.config.min_feed_override = 0.0
            self.status.config.max_spindle_override = 0.0
            self.status.config.min_spindle_override = 0.0
            self.status.config.default_spindle_speed = 0.0
            self.status.config.default_linear_velocity = 0.0
            self.status.config.min_velocity = 0.0
            self.status.config.max_linear_velocity = 0.0
            self.status.config.min_linear_velocity = 0.0
            self.status.config.default_angular_velocity = 0.0
            self.status.config.max_angular_velocity = 0.0
            self.status.config.min_angular_velocity = 0.0
            self.status.config.increments = ""
            self.status.config.grids = ""
            self.status.config.lathe = False
            self.status.config.geometry = ""
            self.status.config.arcdivision = 0
            self.status.config.no_force_homing = False
            self.status.config.remote_path = ""
            self.status.config.time_units = TIME_UNITS_MINUTE
            self.status.config.name = ""
            self.config_first_run = False

            extensions = self.ini.findall("FILTER", "PROGRAM_EXTENSION")
            tx_obj_item = EmcProgramExtension()
            obj = self.status.config.program_extension
            tx_obj = self.status_tx.config.program_extension
            modified |= self.update_proto_list(
                obj, tx_obj, tx_obj_item, 'extension', extensions, ''
            )
            del tx_obj_item

            commands = self.ini.findall("DISPLAY", "USER_COMMAND")
            tx_obj_item = EmcStatusUserCommand()
            obj = self.status.config.user_command
            tx_obj = self.status_tx.config.user_command
            modified |= self.update_proto_list(
                obj, tx_obj, tx_obj_item, 'command', commands, ''
            )
            del tx_obj_item

            position_offset = self.ini.find('DISPLAY', 'POSITION_OFFSET') or 'RELATIVE'
            if position_offset == 'MACHINE':
                position_offset = EMC_CONFIG_MACHINE_OFFSET
            else:
                position_offset = EMC_CONFIG_RELATIVE_OFFSET
            modified |= self.update_config_value('position_offset', position_offset)

            position_feedback = (
                self.ini.find('DISPLAY', 'POSITION_FEEDBACK') or 'ACTUAL'
            )
            if position_feedback == 'COMMANDED':
                position_feedback = EMC_CONFIG_COMMANDED_FEEDBACK
            else:
                position_feedback = EMC_CONFIG_ACTUAL_FEEDBACK
            modified |= self.update_config_value('position_feedback', position_feedback)

            value = float(self.ini.find('DISPLAY', 'MAX_FEED_OVERRIDE') or 1.2)
            modified |= self.update_config_value('max_feed_override', value)

            value = float(self.ini.find('DISPLAY', 'MIN_FEED_OVERRIDE') or 0.5)
            modified |= self.update_config_value('min_feed_override', value)

            value = float(self.ini.find('DISPLAY', 'MAX_SPINDLE_OVERRIDE') or 1.0)
            modified |= self.update_config_value('max_spindle_override', value)

            value = float(self.ini.find('DISPLAY', 'MIN_SPINDLE_OVERRIDE') or 0.5)
            modified |= self.update_config_value('min_spindle_override', value)

            value = float(self.ini.find('DISPLAY', 'DEFAULT_SPINDLE_SPEED') or 1)
            modified |= self.update_config_value('default_spindle_speed', value)

            value = float(self.ini.find('DISPLAY', 'DEFAULT_LINEAR_VELOCITY') or 0.25)
            modified |= self.update_config_value('default_linear_velocity', value)

            value = float(self.ini.find('DISPLAY', 'MIN_VELOCITY') or 0.01)
            modified |= self.update_config_value('min_velocity', value)

            value = float(self.ini.find('DISPLAY', 'MAX_LINEAR_VELOCITY') or 1.00)
            modified |= self.update_config_value('max_linear_velocity', value)

            value = float(self.ini.find('DISPLAY', 'MIN_LINEAR_VELOCITY') or 0.01)
            modified |= self.update_config_value('min_linear_velocity', value)

            value = float(self.ini.find('DISPLAY', 'DEFAULT_ANGULAR_VELOCITY') or 0.25)
            modified |= self.update_config_value('default_angular_velocity', value)

            value = float(self.ini.find('DISPLAY', 'MAX_ANGULAR_VELOCITY') or 1.00)
            modified |= self.update_config_value('max_angular_velocity', value)

            value = float(self.ini.find('DISPLAY', 'MIN_ANGULAR_VELOCITY') or 0.01)
            modified |= self.update_config_value('min_angular_velocity', value)

            value = self.ini.find('DISPLAY', 'INCREMENTS') or '0.001 0.01 0.1 1.0'
            modified |= self.update_config_value('increments', value)

            value = self.ini.find('DISPLAY', 'GRIDS') or ''
            modified |= self.update_config_value('grids', value)

            value = bool(self.ini.find('DISPLAY', 'LATHE') or False)
            modified |= self.update_config_value('lathe', value)

            value = self.ini.find('DISPLAY', 'GEOMETRY') or ''
            modified |= self.update_config_value('geometry', value)

            value = int(self.ini.find('DISPLAY', 'ARCDIVISION') or 64)
            modified |= self.update_config_value('arcdivision', value)

            value = bool(self.ini.find('TRAJ', 'NO_FORCE_HOMING') or False)
            modified |= self.update_config_value('no_force_homing', value)

            value = float(self.ini.find('TRAJ', 'MAX_VELOCITY') or 5.0)
            modified |= self.update_config_value('max_velocity', value)

            value = float(self.ini.find('TRAJ', 'MAX_ACCELERATION') or 20.0)
            modified |= self.update_config_value('max_acceleration', value)

            time_units = str(self.ini.find('DISPLAY', 'TIME_UNITS') or 'min')
            if time_units in ['min', 'minute']:
                time_units_converted = TIME_UNITS_MINUTE
            elif time_units in ['s', 'second']:
                time_units_converted = TIME_UNITS_SECOND
            else:
                time_units_converted = TIME_UNITS_MINUTE
            modified |= self.update_config_value('time_units', time_units_converted)

            linear_units = str(self.ini.find('TRAJ', 'LINEAR_UNITS') or 'mm')
            if linear_units in ['mm', 'metric']:
                linear_units_converted = LINEAR_UNITS_MM
            elif linear_units in ['in', 'inch', 'imperial']:
                linear_units_converted = LINEAR_UNITS_INCH
            elif linear_units in ['cm']:
                linear_units_converted = LINEAR_UNITS_CM
            else:
                linear_units_converted = LINEAR_UNITS_MM
            modified |= self.update_config_value('linear_units', linear_units_converted)

            angular_units = str(self.ini.find('TRAJ', 'ANGULAR_UNITS') or 'deg')
            if angular_units in ['deg', 'degree']:
                angular_units_converted = ANGULAR_UNITS_DEGREES
            elif angular_units in ['rad', 'radian']:
                angular_units_converted = ANGULAR_UNITS_RADIAN
            elif angular_units in ['grad', 'gon']:
                angular_units_converted = ANGULAR_UNITS_GRAD
            else:
                angular_units_converted = ANGULAR_UNITS_DEGREES
            modified |= self.update_config_value(
                'angular_units', angular_units_converted
            )

            modified |= self.update_config_value('remote_path', self.directory)

            name = str(self.ini.find('EMC', 'MACHINE') or '')
            modified |= self.update_config_value('name', name)

        for name in ['axis_mask', 'debug', 'kinematics_type', 'axes']:
            modified |= self.update_config_value(name, getattr(stat, name))

        for name in ['cycle_time']:
            modified |= self.update_config_float(name, getattr(stat, name))

        modified |= self.update_config_float('default_acceleration', stat.acceleration)
        modified |= self.update_config_float('default_velocity', stat.velocity)

        tx_axis = EmcStatusConfigAxis()
        for index, stat_axis in enumerate(stat.axis):
            tx_axis.Clear()
            axis_modified = False

            if index == stat.axes:
                break

            if len(self.status.config.axis) == index:
                self.status.config.axis.add()
                self.status.config.axis[index].index = index
                self.status.config.axis[index].axis_type = EMC_AXIS_LINEAR
                self.status.config.axis[index].backlash = 0.0
                self.status.config.axis[index].max_ferror = 0.0
                self.status.config.axis[index].max_position_limit = 0.0
                self.status.config.axis[index].min_ferror = 0.0
                self.status.config.axis[index].min_position_limit = 0.0
                self.status.config.axis[index].home_sequence = -1
                self.status.config.axis[index].max_velocity = 0.0
                self.status.config.axis[index].max_acceleration = 0.0
                self.status.config.axis[index].increments = ""

                axis = self.status.config.axis[index]
                axis_name = 'AXIS_%i' % index
                value = int(self.ini.find(axis_name, 'HOME_SEQUENCE') or -1)
                axis_modified |= self.update_proto_value(
                    axis, tx_axis, 'home_sequence', value
                )

                value = float(self.ini.find(axis_name, 'MAX_VELOCITY') or 0.0)
                axis_modified |= self.update_proto_value(
                    axis, tx_axis, 'max_velocity', value
                )

                value = float(self.ini.find(axis_name, 'MAX_ACCELERATION') or 0.0)
                axis_modified |= self.update_proto_value(
                    axis, tx_axis, 'max_acceleration', value
                )

                value = self.ini.find(axis_name, 'INCREMENTS') or ''
                axis_modified |= self.update_proto_value(
                    axis, tx_axis, 'increments', value
                )

            axis = self.status.config.axis[index]
            axis_modified |= self.update_proto_value(
                axis, tx_axis, 'axis_type', stat_axis['axisType']
            )

            for name in [
                'backlash',
                'max_ferror',
                'max_position_limit',
                'min_ferror',
                'min_position_limit',
            ]:
                axis_modified |= self.update_proto_float(
                    axis, tx_axis, name, stat_axis[name]
                )

            if axis_modified:
                tx_axis.index = index
                self.status_tx.config.axis.add().CopyFrom(tx_axis)
                modified = True

        del tx_axis

        if self.config_full_update:
            self.add_pparams()
            self.send_config(self.status.config, MT_EMCSTAT_FULL_UPDATE)
            self.config_full_update = False
        elif modified:
            self.send_config(self.status_tx.config, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_io(self, stat):
        modified = False

        if self.io_first_run:
            self.status.io.estop = 0
            self.status.io.flood = 0
            self.status.io.lube = 0
            self.status.io.lube_level = 0
            self.status.io.mist = 0
            self.status.io.pocket_prepped = 0
            self.status.io.tool_in_spindle = 0
            self.status.io.tool_offset.MergeFrom(self.zero_position())
            self.io_first_run = False

        for name in [
            'estop',
            'flood',
            'lube',
            'lube_level',
            'mist',
            'pocket_prepped',
            'tool_in_spindle',
        ]:
            modified |= self.update_io_value(name, getattr(stat, name))

        modified |= self.update_proto_position(
            self.status.io, self.status_tx.io, 'tool_offset', stat.tool_offset
        )

        tx_tool_result = EmcToolData()
        tool_table_changed = False
        table_index = 0
        for index, stat_tool_result in enumerate(stat.tool_table):
            tx_tool_result.Clear()
            result_modified = False
            new_item = False

            if index == 0 and not self.random_tool_changer:
                continue

            if stat_tool_result.id == -1 and not self.random_tool_changer:
                break  # last tool in table, except index = 0 (spindle !)

            if len(self.status.io.tool_table) == table_index:  # item added
                item = self.status.io.tool_table.add()
                item.index = table_index
                item.id = 0
                item.offset.MergeFrom(self.zero_position())
                item.diameter = 0.0
                item.frontangle = 0.0
                item.backangle = 0.0
                item.orientation = 0
                item.comment = ""
                item.pocket = 0
                new_item = True

            tool_result = self.status.io.tool_table[table_index]

            for name in ['id', 'orientation']:
                value = getattr(stat_tool_result, name)
                result_modified |= self.update_proto_value(
                    tool_result, tx_tool_result, name, value
                )
            for name in ['diameter', 'frontangle', 'backangle']:
                value = getattr(stat_tool_result, name)
                result_modified |= self.update_proto_float(
                    tool_result, tx_tool_result, name, value
                )

            position = range(0, 9)
            for i, axis in enumerate(['x', 'y', 'z', 'a', 'b', 'c', 'u', 'v', 'w']):
                value = getattr(stat_tool_result, axis + 'offset')
                position[i] = value
            result_modified |= self.update_proto_position(
                tool_result, tx_tool_result, 'offset', position
            )

            if result_modified:
                tx_tool_result.index = table_index
                if new_item:
                    self.status_tx.io.tool_table.add().CopyFrom(
                        tool_result
                    )  # make sure to send update
                else:
                    self.status_tx.io.tool_table.add().CopyFrom(tx_tool_result)
                modified = True
                tool_table_changed = True

            table_index += 1

        # cleanup dead entries
        while table_index < len(self.status.io.tool_table):
            del self.status.io.tool_table[-1]

        # check if new tool table is smaller
        # if so we need to send empty messages (only index) to the subscribers
        if table_index < self.io_tool_table_count:
            for i in range(table_index, self.io_tool_table_count):
                tx_tool_result.Clear()
                tx_tool_result.index = i
                self.status_tx.io.tool_table.add().CopyFrom(tx_tool_result)
            tool_table_changed = True
        self.io_tool_table_count = table_index

        if tool_table_changed or self.io_tool_table_loaded:
            # update pocket and comment from tool table file
            self.load_tool_table(self.status.io, self.status_tx.io)
            self.io_tool_table_loaded = False
            modified = True
        del tx_tool_result

        if self.io_full_update:
            self.add_pparams()
            self.send_io(self.status.io, MT_EMCSTAT_FULL_UPDATE)
            self.io_full_update = False
        elif modified:
            self.send_io(self.status_tx.io, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_task(self, stat):
        modified = False

        if self.task_first_run:
            self.status.task.echo_serial_number = 0
            self.status.task.exec_state = EMC_TASK_EXEC_ERROR
            self.status.task.file = ""
            self.status.task.input_timeout = False
            self.status.task.optional_stop = False
            self.status.task.read_line = 0
            self.status.task.task_mode = EMC_TASK_MODE_MANUAL
            self.status.task.task_paused = 0
            self.status.task.task_state = EMC_TASK_STATE_ESTOP
            self.status.task.total_lines = 0
            self.task_first_run = False

        for name in [
            'echo_serial_number',
            'exec_state',
            'file',
            'input_timeout',
            'optional_stop',
            'read_line',
            'task_mode',
            'task_paused',
            'task_state',
        ]:
            modified |= self.update_task_value(name, getattr(stat, name))

        modified |= self.update_task_value('total_lines', self.total_lines)

        if self.task_full_update:
            self.add_pparams()
            self.send_task(self.status.task, MT_EMCSTAT_FULL_UPDATE)
            self.task_full_update = False
        elif modified:
            self.send_task(self.status_tx.task, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_interp(self, stat):
        modified = False

        if self.interp_first_run:
            self.status.interp.command = ""
            self.status.interp.interp_state = EMC_TASK_INTERP_IDLE
            self.status.interp.interpreter_errcode = 0
            self.status.interp.program_units = CANON_UNITS_INCH
            self.interp_first_run = False

        for name in ['command', 'interp_state', 'interpreter_errcode', 'program_units']:
            modified |= self.update_interp_value(name, getattr(stat, name))

        tx_obj_item = EmcStatusGCode()
        obj = self.status.interp.gcodes
        tx_obj = self.status_tx.interp.gcodes
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.gcodes, 0
        )
        del tx_obj_item

        tx_obj_item = EmcStatusMCode()
        obj = self.status.interp.mcodes
        tx_obj = self.status_tx.interp.mcodes
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.mcodes, 0
        )
        del tx_obj_item

        tx_obj_item = EmcStatusSetting()
        obj = self.status.interp.settings
        tx_obj = self.status_tx.interp.settings
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.settings, 0.0
        )
        del tx_obj_item

        if self.interp_full_update:
            self.add_pparams()
            self.send_interp(self.status.interp, MT_EMCSTAT_FULL_UPDATE)
            self.interp_full_update = False
        elif modified:
            self.send_interp(self.status_tx.interp, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_motion(self, stat):
        modified = False

        if self.motion_first_run:
            self.status.motion.active_queue = 0
            self.status.motion.actual_position.MergeFrom(self.zero_position())
            self.status.motion.adaptive_feed_enabled = False
            self.status.motion.block_delete = False
            self.status.motion.current_line = 0
            self.status.motion.current_vel = 0.0
            self.status.motion.delay_left = 0.0
            self.status.motion.distance_to_go = 0.0
            self.status.motion.dtg.MergeFrom(self.zero_position())
            self.status.motion.enabled = False
            self.status.motion.feed_hold_enabled = False
            self.status.motion.feed_override_enabled = False
            self.status.motion.feedrate = 0.0
            self.status.motion.rapidrate = 0.0
            self.status.motion.g5x_index = ORIGIN_G54
            self.status.motion.g5x_offset.MergeFrom(self.zero_position())
            self.status.motion.g92_offset.MergeFrom(self.zero_position())
            self.status.motion.id = 0
            self.status.motion.inpos = False
            self.status.motion.joint_actual_position.MergeFrom(self.zero_position())
            self.status.motion.joint_position.MergeFrom(self.zero_position())
            self.status.motion.motion_line = 0
            self.status.motion.motion_type = 0
            self.status.motion.motion_mode = EMC_TRAJ_MODE_FREE
            self.status.motion.paused = False
            self.status.motion.position.MergeFrom(self.zero_position())
            self.status.motion.probe_tripped = False
            self.status.motion.probe_val = 0
            self.status.motion.probed_position.MergeFrom(self.zero_position())
            self.status.motion.probing = False
            self.status.motion.queue = 0
            self.status.motion.queue_full = False
            self.status.motion.rotation_xy = 0.0
            self.status.motion.spindle_brake = 0
            self.status.motion.spindle_direction = 0
            self.status.motion.spindle_enabled = 0
            self.status.motion.spindle_increasing = 0
            self.status.motion.spindle_override_enabled = False
            self.status.motion.spindle_speed = 0.0
            self.status.motion.spindlerate = 0.0
            self.status.motion.state = UNINITIALIZED_STATUS
            self.status.motion.max_velocity = 0.0
            self.status.motion.max_acceleration = 0.0
            self.motion_first_run = False

        for name in [
            'active_queue',
            'adaptive_feed_enabled',
            'block_delete',
            'current_line',
            'enabled',
            'feed_hold_enabled',
            'feed_override_enabled',
            'g5x_index',
            'id',
            'inpos',
            'motion_line',
            'motion_type',
            'motion_mode',
            'paused',
            'probe_tripped',
            'probe_val',
            'probing',
            'queue',
            'queue_full',
            'spindle_brake',
            'spindle_direction',
            'spindle_enabled',
            'spindle_increasing',
            'spindle_override_enabled',
            'state',
        ]:
            modified |= self.update_motion_value(name, getattr(stat, name))

        for name in [
            'current_vel',
            'delay_left',
            'distance_to_go',
            'feedrate',
            'rapidrate',
            'rotation_xy',
            'spindle_speed',
            'spindlerate',
            'max_acceleration',
            'max_velocity',
        ]:
            modified |= self.update_motion_float(name, getattr(stat, name))

        for name in [
            'actual_position',
            'dtg',
            'g5x_offset',
            'g92_offset',
            'joint_actual_position',
            'joint_position',
            'position',
            'probed_position',
        ]:
            modified |= self.update_proto_position(
                self.status.motion, self.status_tx.motion, name, getattr(stat, name)
            )

        tx_obj_item = EmcStatusAnalogIO()
        obj = self.status.motion.ain
        tx_obj = self.status_tx.motion.ain
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.ain, 0.0
        )
        del tx_obj_item

        tx_obj_item = EmcStatusAnalogIO()
        obj = self.status.motion.aout
        tx_obj = self.status_tx.motion.aout
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.aout, 0.0
        )
        del tx_obj_item

        tx_obj_item = EmcStatusDigitalIO()
        obj = self.status.motion.din
        tx_obj = self.status_tx.motion.din
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.din, False
        )
        del tx_obj_item

        tx_obj_item = EmcStatusDigitalIO()
        obj = self.status.motion.dout
        tx_obj = self.status_tx.motion.dout
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.dout, False
        )
        del tx_obj_item

        tx_obj_item = EmcStatusLimit()
        obj = self.status.motion.limit
        tx_obj = self.status_tx.motion.limit
        modified |= self.update_proto_list(
            obj, tx_obj, tx_obj_item, 'value', stat.limit, False
        )
        del tx_obj_item

        tx_axis = EmcStatusMotionAxis()
        for index, stat_axis in enumerate(stat.axis):
            tx_axis.Clear()
            axis_modified = False

            if index == stat.axes:
                break

            if len(self.status.motion.axis) == index:
                self.status.motion.axis.add()
                self.status.motion.axis[index].index = index
                self.status.motion.axis[index].enabled = False
                self.status.motion.axis[index].fault = False
                self.status.motion.axis[index].ferror_current = 0.0
                self.status.motion.axis[index].ferror_highmark = 0.0
                self.status.motion.axis[index].homed = False
                self.status.motion.axis[index].homing = False
                self.status.motion.axis[index].inpos = False
                self.status.motion.axis[index].input = 0.0
                self.status.motion.axis[index].max_hard_limit = False
                self.status.motion.axis[index].max_soft_limit = False
                self.status.motion.axis[index].min_hard_limit = False
                self.status.motion.axis[index].min_soft_limit = False
                self.status.motion.axis[index].output = 0.0
                self.status.motion.axis[index].override_limits = False
                self.status.motion.axis[index].velocity = 0.0

            axis = self.status.motion.axis[index]
            for name in [
                'enabled',
                'fault',
                'homed',
                'homing',
                'inpos',
                'max_hard_limit',
                'max_soft_limit',
                'min_hard_limit',
                'min_soft_limit',
                'override_limits',
            ]:
                axis_modified |= self.update_proto_value(
                    axis, tx_axis, name, stat_axis[name]
                )

            for name in [
                'ferror_current',
                'ferror_highmark',
                'input',
                'output',
                'velocity',
            ]:
                axis_modified |= self.update_proto_float(
                    axis, tx_axis, name, stat_axis[name]
                )

            if axis_modified:
                tx_axis.index = index
                self.status_tx.motion.axis.add().CopyFrom(tx_axis)
                modified = True
        del tx_axis

        if self.motion_full_update:
            self.add_pparams()
            self.send_motion(self.status.motion, MT_EMCSTAT_FULL_UPDATE)
            self.motion_full_update = False
        elif modified:
            self.send_motion(self.status_tx.motion, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_ui(self, _stat):
        modified = False

        if self.ui_first_run:
            self.status.ui.spindle_brake_visible = False
            self.status.ui.spindle_cw_visible = False
            self.status.ui.spindle_ccw_visible = False
            self.status.ui.spindle_stop_visible = False
            self.status.ui.spindle_plus_visible = False
            self.status.ui.spindle_minus_visible = False
            self.status.ui.spindle_override_visible = False
            self.status.ui.coolant_flood_visible = False
            self.status.ui.coolant_mist_visible = False

            modified |= self.update_ui_value(
                'spindle_brake_visible',
                self.get_ui_element_visible("motion.spindle-brake"),
            )
            modified |= self.update_ui_value(
                'spindle_cw_visible',
                self.get_ui_element_visible(
                    "motion.spindle-forward",
                    "motion.spindle-on",
                    "motion.spindle-speed-out",
                    "motion.spindle-speed-out-abs",
                    "motion.spindle-speed-out-rps",
                    "motion.spindle-speed-out-rps-abs",
                ),
            )
            modified |= self.update_ui_value(
                'spindle_ccw_visible',
                self.get_ui_element_visible(
                    "motion.spindle-reverse",
                    "motion.spindle-speed-out",
                    "motion.spindle-speed-out-abs",
                    "motion.spindle-speed-out-rps",
                    "motion.spindle-speed-out-rps-abs",
                ),
            )
            modified |= self.update_ui_value(
                'spindle_stop_visible',
                self.get_ui_element_visible(
                    "motion.spindle-forward",
                    "motion.spindle-reverse",
                    "motion.spindle-on",
                    "motion.spindle-speed-out",
                    "motion.spindle-speed-out-abs",
                    "motion.spindle-speed-out-rps",
                    "motion.spindle-speed-out-rps-abs",
                ),
            )
            modified |= self.update_ui_value(
                'spindle_plus_visible',
                self.get_ui_element_visible(
                    "motion.spindle-speed-out",
                    "motion.spindle-speed-out-abs",
                    "motion.spindle-speed-out-rps",
                    "motion.spindle-speed-out-rps-abs",
                ),
            )
            modified |= self.update_ui_value(
                'spindle_minus_visible',
                self.get_ui_element_visible(
                    "motion.spindle-speed-out",
                    "motion.spindle-speed-out-abs",
                    "motion.spindle-speed-out-rps",
                    "motion.spindle-speed-out-rps-abs",
                ),
            )
            modified |= self.update_ui_value(
                'spindle_override_visible',
                self.get_ui_element_visible(
                    "motion.spindle-speed-out",
                    "motion.spindle-speed-out-abs",
                    "motion.spindle-speed-out-rps",
                    "motion.spindle-speed-out-rps-abs",
                ),
            )
            modified |= self.update_ui_value(
                'coolant_flood_visible',
                self.get_ui_element_visible("iocontrol.0.coolant-flood"),
            )
            modified |= self.update_ui_value(
                'coolant_mist_visible',
                self.get_ui_element_visible("iocontrol.0.coolant-mist"),
            )

        if self.ui_full_update:
            self.add_pparams()
            self.send_ui(self.status.ui, MT_EMCSTAT_FULL_UPDATE)
            self.ui_full_update = False
        elif modified:
            self.send_ui(self.status_tx.ui, MT_EMCSTAT_INCREMENTAL_UPDATE)

    @staticmethod
    def get_ui_element_visible(*hal_pins):
        for name in hal_pins:
            if hal.pins[name].linked:
                return True
        return False

    def update_status(self, stat):
        self.status_tx.clear()
        if self.io_subscribed:
            self.update_io(stat)
        if self.task_subscribed:
            self.update_task(stat)
        if self.interp_subscribed:
            self.update_interp(stat)
        if self.motion_subscribed:
            self.update_motion(stat)
        if self.config_subscribed:
            self.update_config(stat)
        if self.ui_subscribed:
            self.update_ui(stat)

    def update_error(self, error):
        with self.error_note_lock:
            for linuxcnc_error in self.linuxcnc_errors:
                self.tx_error.note.append(linuxcnc_error)
                self.send_error_msg('error', MT_EMC_NML_ERROR)
            self.linuxcnc_errors = []

        if not error:
            return

        kind, text = error
        text = text.encode('utf-8')
        self.tx_error.note.append(text)

        if kind == linuxcnc.NML_ERROR:
            if self.error_subscribed:
                self.send_error_msg('error', MT_EMC_NML_ERROR)
        elif kind == linuxcnc.OPERATOR_ERROR:
            if self.error_subscribed:
                self.send_error_msg('error', MT_EMC_OPERATOR_ERROR)
        elif kind == linuxcnc.NML_TEXT:
            if self.text_subscribed:
                self.send_error_msg('text', MT_EMC_NML_TEXT)
        elif kind == linuxcnc.OPERATOR_TEXT:
            if self.text_subscribed:
                self.send_error_msg('text', MT_EMC_OPERATOR_TEXT)
        elif kind == linuxcnc.NML_DISPLAY:
            if self.display_subscribed:
                self.send_error_msg('display', MT_EMC_NML_DISPLAY)
        elif kind == linuxcnc.OPERATOR_DISPLAY:
            if self.display_subscribed:
                self.send_error_msg('display', MT_EMC_OPERATOR_DISPLAY)

    def add_error(self, note):
        with self.error_note_lock:
            self.linuxcnc_errors.append(note)

    def preview_error(self, error, line):
        self.add_error("%s\non line %s" % (error, str(line)))

    def send_config(self, data, type_):
        self.tx_status.emc_status_config.MergeFrom(data)
        if self.debug:
            print("sending config message")
        self.send_status_msg('config', type_)

    def send_io(self, data, type_):
        self.tx_status.emc_status_io.MergeFrom(data)
        if self.debug:
            print("sending io message")
        self.send_status_msg('io', type_)

    def send_task(self, data, type_):
        self.tx_status.emc_status_task.MergeFrom(data)
        if self.debug:
            print("sending task message")
        self.send_status_msg('task', type_)

    def send_motion(self, data, type_):
        self.tx_status.emc_status_motion.MergeFrom(data)
        if self.debug:
            print("sending motion message")
        self.send_status_msg('motion', type_)

    def send_interp(self, data, type_):
        self.tx_status.emc_status_interp.MergeFrom(data)
        if self.debug:
            print("sending interp message")
        self.send_status_msg('interp', type_)

    def send_ui(self, data, type_):
        self.tx_status.emc_status_ui.MergeFrom(data)
        if self.debug:
            print("sending ui message")
        self.send_status_msg('ui', type_)

    def send_status_msg(self, topic, type_):
        with self.status_lock:
            self.tx_status.type = type_
            tx_buffer = self.tx_status.SerializeToString()
            self.status_socket.send_multipart([topic, tx_buffer], zmq.NOBLOCK)
            self.tx_status.Clear()

    def send_error_msg(self, topic, type_):
        with self.error_lock:
            self.tx_error.type = type_
            tx_buffer = self.tx_error.SerializeToString()
            self.error_socket.send_multipart([topic, tx_buffer], zmq.NOBLOCK)
            self.tx_error.Clear()

    def send_command_msg(self, identity, type_):
        with self.command_lock:
            self.tx_command.type = type_
            tx_buffer = self.tx_command.SerializeToString()
            self.command_socket.send_multipart(identity + [tx_buffer], zmq.NOBLOCK)
            self.tx_command.Clear()

    def add_pparams(self):
        parameters = ProtocolParameters()
        parameters.keepalive_timer = int(self.ping_interval * 1000.0)
        self.tx_status.pparams.MergeFrom(parameters)

    def ping_status(self):
        if self.io_subscribed:
            self.send_status_msg('io', MT_PING)
        if self.task_subscribed:
            self.send_status_msg('task', MT_PING)
        if self.interp_subscribed:
            self.send_status_msg('interp', MT_PING)
        if self.motion_subscribed:
            self.send_status_msg('motion', MT_PING)
        if self.config_subscribed:
            self.send_status_msg('config', MT_PING)
        if self.ui_subscribed:
            self.send_status_msg('ui', MT_PING)

    def ping_error(self):
        if self.new_error_subscription:  # not very clear
            self.add_pparams()
            self.new_error_subscription = False

        if self.error_subscribed:
            self.send_error_msg('error', MT_PING)
        if self.text_subscribed:
            self.send_error_msg('text', MT_PING)
        if self.display_subscribed:
            self.send_error_msg('display', MT_PING)

    def process_status(self, zmq_socket):
        try:
            with self.status_lock:
                rc = zmq_socket.recv()
            subscription = rc[1:]
            status = rc[0] == "\x01"

            if subscription == 'motion':
                self.motion_subscribed = status
                self.motion_full_update = status
            elif subscription == 'task':
                self.task_subscribed = status
                self.task_full_update = status
            elif subscription == 'io':
                self.io_subscribed = status
                self.io_full_update = status
            elif subscription == 'config':
                self.config_subscribed = status
                self.config_full_update = status
            elif subscription == 'interp':
                self.interp_subscribed = status
                self.interp_full_update = status
            elif subscription == 'ui':
                self.ui_subscribed = status
                self.ui_full_update = status

            self.status_service_subscribed = (
                self.motion_subscribed
                or self.task_subscribed
                or self.io_subscribed
                or self.config_subscribed
                or self.interp_subscribed
                or self.ui_subscribed
            )

            if self.debug:
                print("process status called {} {}".format(subscription, status))
                print(
                    "status service subscribed: {}".format(
                        self.status_service_subscribed
                    )
                )

        except zmq.ZMQError as e:
            print_error('ZMQ error: {}'.format(e))

    def process_error(self, zmq_socket):
        try:
            with self.error_lock:
                rc = zmq_socket.recv()
            subscription = rc[1:]
            status = rc[0] == "\x01"

            if subscription == 'error':
                self.new_error_subscription = status
                self.error_subscribed = status
            elif subscription == 'text':
                self.new_error_subscription = status
                self.text_subscribed = status
            elif subscription == 'display':
                self.new_error_subscription = status
                self.display_subscribed = status

            self.error_service_subscribed = (
                self.error_subscribed or self.text_subscribed or self.display_subscribed
            )

            if self.debug:
                print("process error called {} {}".format(subscription, status))
                print(
                    "error service subscribed: {}".format(self.error_service_subscribed)
                )

        except zmq.ZMQError as e:
            print_error('ZMQ error: {}'.format(e))

    def get_active_gcodes(self):
        raw_gcodes = self.stat.gcodes
        gcodes = []
        for rawGCode in raw_gcodes:
            if rawGCode > -1:
                gcodes.append('G' + str(rawGCode / 10.0))
        return ' '.join(gcodes)

    def send_command_wrong_params(self, identity, note="wrong parameters"):
        self.tx_command.note.append(note)
        self.send_command_msg(identity, MT_ERROR)

    def send_command_completed(self, identity, ticket):
        self.tx_command.reply_ticket = ticket
        self.send_command_msg(identity, MT_EMCCMD_COMPLETED)

    def send_command_executed(self, identity, ticket):
        self.tx_command.reply_ticket = ticket
        self.send_command_msg(identity, MT_EMCCMD_EXECUTED)

    def command_completion_process(self, event):
        self.command.wait_complete()  # wait for emcmodule
        event.set()  # inform the listening thread

    def command_completion_thread(self, identity, ticket):
        event = multiprocessing.Event()
        # wait in separate process to prevent GIL from causing problems
        multiprocessing.Process(
            target=self.command_completion_process, args=(event,)
        ).start()
        # wait until the command is completed
        event.wait()
        self.send_command_completed(identity, ticket)

        if self.debug:
            print('command #{} from {} completed'.format(ticket, identity))

    def wait_complete(self, identity, ticket):
        self.send_command_executed(identity, ticket)

        if self.debug:
            print(
                'waiting for command #{} from {} to complete'.format(ticket, identity)
            )

        # kick off the monitoring thread
        threading.Thread(
            target=self.command_completion_thread, args=(identity, ticket)
        ).start()

    def process_command(self, zmq_socket):
        with self.command_lock:
            frames = zmq_socket.recv_multipart()
            identity = frames[:-1]  # multipart id
            message = frames[-1]  # last frame

        if self.debug:
            print("process command called, id: {}".format(identity))

        try:
            self.rx.ParseFromString(message)
        except DecodeError as e:
            note = 'Protobuf Decode Error: {}'.format(e)
            self.send_command_wrong_params(identity, note=note)
            return

        try:
            if self.rx.type == MT_PING:
                self.send_command_msg(identity, MT_PING_ACKNOWLEDGE)

            elif self.rx.type == MT_SHUTDOWN:
                self.send_command_msg(identity, MT_CONFIRM_SHUTDOWN)
                self.stop()  # trigger the shutdown event

            elif self.rx.type == MT_EMC_TASK_ABORT:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.abort()
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                    elif self.rx.interp_name == 'preview':
                        self.preview.abort()
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_PAUSE:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.auto(linuxcnc.AUTO_PAUSE)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_RESUME:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.auto(linuxcnc.AUTO_RESUME)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_STEP:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.auto(linuxcnc.AUTO_STEP)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_RUN:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('line_number')
                    and self.rx.HasField('interp_name')
                ):
                    if self.rx.interp_name == 'execute':
                        line_number = self.rx.emc_command_params.line_number
                        self.command.auto(linuxcnc.AUTO_RUN, line_number)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                    elif self.rx.interp_name == 'preview':
                        self.preview.start()
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_SPINDLE_BRAKE_ENGAGE:
                self.command.brake(linuxcnc.BRAKE_ENGAGE)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_SPINDLE_BRAKE_RELEASE:
                self.command.brake(linuxcnc.BRAKE_RELEASE)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_SET_DEBUG:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('debug_level'):
                    debug_level = self.rx.emc_command_params.debug_level
                    self.command.debug(debug_level)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_SCALE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('scale'):
                    feedrate = self.rx.emc_command_params.scale
                    self.command.feedrate(feedrate)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_RAPID_SCALE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('scale'):
                    rapidrate = self.rx.emc_command_params.scale
                    self.command.rapidrate(rapidrate)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_COOLANT_FLOOD_ON:
                self.command.flood(linuxcnc.FLOOD_ON)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_COOLANT_FLOOD_OFF:
                self.command.flood(linuxcnc.FLOOD_OFF)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_AXIS_HOME:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.home(axis)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_ABORT:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.jog(linuxcnc.JOG_STOP, axis)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_JOG:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('index')
                    and self.rx.emc_command_params.HasField('velocity')
                ):
                    axis = self.rx.emc_command_params.index
                    velocity = self.rx.emc_command_params.velocity
                    self.command.jog(linuxcnc.JOG_CONTINUOUS, axis, velocity)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_INCR_JOG:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('index')
                    and self.rx.emc_command_params.HasField('velocity')
                    and self.rx.emc_command_params.HasField('distance')
                ):
                    axis = self.rx.emc_command_params.index
                    velocity = self.rx.emc_command_params.velocity
                    distance = self.rx.emc_command_params.distance
                    self.command.jog(linuxcnc.JOG_INCREMENT, axis, velocity, distance)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_MAX_VELOCITY:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('velocity'):
                    velocity = self.rx.emc_command_params.velocity
                    self.command.maxvel(velocity)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_EXECUTE:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('command')
                    and self.rx.HasField('interp_name')
                ):
                    if self.rx.interp_name == 'execute':
                        command = self.rx.emc_command_params.command
                        self.command.mdi(command)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_COOLANT_MIST_ON:
                self.command.mist(linuxcnc.MIST_ON)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_COOLANT_MIST_OFF:
                self.command.mist(linuxcnc.MIST_OFF)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_TASK_SET_MODE:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('task_mode')
                    and self.rx.HasField('interp_name')
                ):
                    if self.rx.interp_name == 'execute':
                        self.command.mode(self.rx.emc_command_params.task_mode)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_OVERRIDE_LIMITS:
                self.command.override_limits()
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_TASK_PLAN_OPEN:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('path')
                    and self.rx.HasField('interp_name')
                ):
                    file_name = self.rx.emc_command_params.path
                    file_name = self.preprocess_program(file_name)
                    if file_name is not '':
                        if self.rx.interp_name == 'execute':
                            self.command.program_open(file_name)
                            if self.rx.HasField('ticket'):
                                self.wait_complete(identity, self.rx.ticket)
                        elif self.rx.interp_name == 'preview':
                            if self.rx.HasField('ticket'):
                                self.send_command_executed(identity, self.rx.ticket)
                            self.preview.program_open(file_name)
                            if self.rx.HasField('ticket'):
                                self.send_command_completed(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_INIT:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.reset_interpreter()
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_MOTION_ADAPTIVE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    adaptive_feed = self.rx.emc_command_params.enable
                    self.command.set_adaptive_feed(adaptive_feed)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_MOTION_SET_AOUT:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('index')
                    and self.rx.emc_command_params.HasField('value')
                ):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_analog_output(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_SET_BLOCK_DELETE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    block_delete = self.rx.emc_command_params.enable
                    self.command.set_block_delete(block_delete)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_MOTION_SET_DOUT:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('index')
                    and self.rx.emc_command_params.HasField('enable')
                ):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.enable
                    self.command.set_digital_output(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_FH_ENABLE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    feed_hold = self.rx.emc_command_params.enable
                    self.command.set_feed_hold(feed_hold)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_FO_ENABLE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    feed_override = self.rx.emc_command_params.enable
                    self.command.set_feed_override(feed_override)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_SET_MAX_POSITION_LIMIT:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('index')
                    and self.rx.emc_command_params.HasField('value')
                ):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_max_limit(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_SET_MIN_POSITION_LIMIT:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('index')
                    and self.rx.emc_command_params.HasField('value')
                ):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_min_limit(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_SET_OPTIONAL_STOP:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    optional_stop = self.rx.emc_command_params.enable
                    self.command.set_optional_stop(optional_stop)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_SO_ENABLE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    spindle_override = self.rx.emc_command_params.enable
                    self.command.set_spindle_override(spindle_override)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_SPINDLE_ON:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('velocity'):
                    speed = self.rx.emc_command_params.velocity
                    direction = (
                        linuxcnc.SPINDLE_FORWARD
                    )  # always forward, speed can be signed
                    self.command.spindle(direction, speed)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_SPINDLE_INCREASE:
                self.command.spindle(linuxcnc.SPINDLE_INCREASE)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_SPINDLE_DECREASE:
                self.command.spindle(linuxcnc.SPINDLE_DECREASE)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_SPINDLE_CONSTANT:
                self.command.spindle(linuxcnc.SPINDLE_CONSTANT)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_SPINDLE_OFF:
                self.command.spindle(linuxcnc.SPINDLE_OFF)
                if self.rx.HasField('ticket'):
                    self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_TRAJ_SET_SPINDLE_SCALE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('scale'):
                    scale = self.rx.emc_command_params.scale
                    self.command.spindleoverride(scale)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_SET_STATE:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('task_state')
                    and self.rx.HasField('interp_name')
                ):
                    if self.rx.interp_name == 'execute':
                        self.command.state(self.rx.emc_command_params.task_state)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_TELEOP_ENABLE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('enable'):
                    teleop_enable = self.rx.emc_command_params.enable
                    self.command.teleop_enable(teleop_enable)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_TELEOP_VECTOR:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('pose')
                    and self.rx.emc_command_params.pose.HasField('a')
                    and self.rx.emc_command_params.pose.HasField('b')
                    and self.rx.emc_command_params.pose.HasField('c')
                ):
                    a = self.rx.emc_command_params.pose.a
                    b = self.rx.emc_command_params.pose.b
                    c = self.rx.emc_command_params.pose.c
                    if self.rx.emc_command_params.pose.HasField('u'):
                        u = self.rx.emc_command_params.pose.u
                        if self.rx.emc_command_params.pose.HasField('v'):
                            v = self.rx.emc_command_params.pose.v
                            if self.rx.emc_command_params.pose.HasField('w'):
                                w = self.rx.emc_command_params.pose.w
                                self.command.teleop_vector(a, b, c, u, v, w)
                            else:
                                self.command.teleop_vector(a, b, c, u, v)
                        else:
                            self.command.teleop_vector(a, b, c, u)
                    else:
                        self.command.teleop_vector(a, b, c)

                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TOOL_LOAD_TOOL_TABLE:
                self.command.load_tool_table()
                self.command.wait_complete()  # we need to wait for stat to be updated
                self.io_tool_table_loaded = True
                if self.rx.HasField('ticket'):
                    self.send_command_completed(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_TOOL_SET_OFFSET:
                if (
                    self.rx.HasField('emc_command_params')
                    and self.rx.emc_command_params.HasField('tool_data')
                    and self.rx.emc_command_params.tool_data.HasField('offset')
                    and self.rx.emc_command_params.tool_data.HasField('index')
                    and self.rx.emc_command_params.tool_data.offset.HasField('z')
                    and self.rx.emc_command_params.tool_data.offset.HasField('x')
                    and self.rx.emc_command_params.tool_data.HasField('diameter')
                    and self.rx.emc_command_params.tool_data.HasField('frontangle')
                    and self.rx.emc_command_params.tool_data.HasField('backangle')
                    and self.rx.emc_command_params.tool_data.HasField('orientation')
                ):
                    toolno = self.rx.emc_command_params.tool_data.index
                    z_offset = self.rx.emc_command_params.tool_data.offset.z
                    x_offset = self.rx.emc_command_params.tool_data.offset.x
                    diameter = self.rx.emc_command_params.tool_data.diameter
                    frontangle = self.rx.emc_command_params.tool_data.frontangle
                    backangle = self.rx.emc_command_params.tool_data.backangle
                    orientation = self.rx.emc_command_params.tool_data.orientation
                    self.command.tool_offset(
                        toolno,
                        z_offset,
                        x_offset,
                        diameter,
                        frontangle,
                        backangle,
                        orientation,
                    )
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TOOL_UPDATE_TOOL_TABLE:
                if self.rx.HasField('emc_command_params'):
                    if self.rx.HasField('ticket'):
                        self.send_command_executed(identity, self.rx.ticket)
                    if not self.update_tool_table(
                        self.rx.emc_command_params.tool_table
                    ):
                        self.add_error('Cannot update tool table')
                    if self.rx.HasField('ticket'):
                        self.send_command_completed(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_MODE:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('traj_mode'):
                    self.command.traj_mode(self.rx.emc_command_params.traj_mode)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_UNHOME:
                if self.rx.HasField(
                    'emc_command_params'
                ) and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.unhome(axis)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            else:
                self.tx_command.note.append("unknown command")
                self.send_command_msg(identity, MT_ERROR)

        except linuxcnc.error as detail:
            self.add_error(detail)
        except UnicodeEncodeError:
            self.add_error("Please use only ASCII characters")
        except Exception as e:
            print_error('uncaught exception ' + str(e))
            self.add_error(str(e))


shutdown = False


def _exit_handler(signum, frame):
    del signum
    del frame
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
        description='Mkwrapper is wrapper around the linuxcnc python module'
        'as temporary workaround for Machinetalk based user interfaces'
    )
    parser.add_argument('-ini', help='INI file', default=None)
    parser.add_argument('-d', '--debug', help='Enable debug mode', action='store_true')

    args = parser.parse_args()

    debug = args.debug
    ini_file = args.ini

    mkconfig = config.Config()
    mkini = os.getenv("MACHINEKIT_INI")
    if mkini is None:
        mkini = mkconfig.MACHINEKIT_INI
    if not os.path.isfile(mkini):
        sys.stderr.write("MACHINEKIT_INI " + mkini + " does not exist\n")
        sys.exit(1)

    mki = configparser.ConfigParser()
    mki.read(mkini)
    mk_uuid = mki.get("MACHINEKIT", "MKUUID")
    remote = mki.getint("MACHINEKIT", "REMOTE")

    if remote == 0:
        print(
            "Remote communication is deactivated, mkwrapper will use the loopback interfaces"
        )
        print("set REMOTE in " + mkini + " to 1 to enable remote communication")

    if debug:
        print("announcing mkwrapper")

    context = zmq.Context()
    context.linger = 0

    register_exit_handler()

    file_service = None
    mkwrapper = None
    try:
        hostname = '%(fqdn)s'  # replaced by service announcement
        file_service = FileService(
            ini_file=ini_file,
            svc_uuid=mk_uuid,
            host=hostname,
            loopback=(not remote),
            debug=debug,
        )
        file_service.start()

        mkwrapper = LinuxCNCWrapper(
            context,
            host=hostname,
            loopback=(not remote),
            ini_file=ini_file,
            svc_uuid=mk_uuid,
            debug=debug,
        )

        while file_service.running and mkwrapper.running and not check_exit():
            time.sleep(1)
    except Exception as e:
        print_error("uncaught exception: " + str(e))

    if debug:
        print("stopping threads")
    if file_service is not None:
        file_service.stop()
    if mkwrapper is not None:
        mkwrapper.stop()

    # wait for all threads to terminate
    while threading.active_count() > 2:  # one thread for every process is left
        time.sleep(0.1)

    if debug:
        print("threads stopped")
    sys.exit(0)


if __name__ == "__main__":
    main()
