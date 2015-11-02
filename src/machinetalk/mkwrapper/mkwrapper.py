#!/usr/bin/python
import os
import sys
from stat import *
import zmq
import threading
import multiprocessing
import time
import math
import socket
import signal
import argparse
from urlparse import urlparse
import shutil
import subprocess

import ConfigParser
import linuxcnc
from machinekit import service
from machinekit import config

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

from message_pb2 import Container
from config_pb2 import *
from types_pb2 import *
from status_pb2 import *
from preview_pb2 import *
from motcmds_pb2 import *
from object_pb2 import ProtocolParameters


def printError(msg):
    sys.stderr.write('ERROR: ' + msg + '\n')


def getFreePort():
    sock = socket.socket()
    sock.bind(('', 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


class CustomFTPHandler(FTPHandler):

    def on_file_received(self, file):
        # do something when a file has been received
        pass  # TODO inform client about new file

    def on_incomplete_file_received(self, file):
        # remove partially uploaded files
        os.remove(file)


class FileService(threading.Thread):

    def __init__(self, iniFile=None, host='', svcUuid=None,
                loopback=False, debug=False):
        self.debug = debug
        self.host = host
        self.loopback = loopback
        self.shutdown = threading.Event()
        self.running = False

        # Linuxcnc
        try:
            iniFile = iniFile or os.environ.get('INI_FILE_NAME', '/dev/null')
            self.ini = linuxcnc.ini(iniFile)
            self.directory = self.ini.find('DISPLAY', 'PROGRAM_PREFIX') or os.getcwd()
            self.directory = os.path.expanduser(self.directory)
        except linuxcnc.error as detail:
            printError(str(detail))
            sys.exit(1)

        self.filePort = getFreePort()
        self.fileDsname = "ftp://" + self.host + ":" + str(self.filePort)

        self.fileService = service.Service(type='file',
                                   svcUuid=svcUuid,
                                   dsn=self.fileDsname,
                                   port=self.filePort,
                                   host=self.host,
                                   loopback=self.loopback,
                                   debug=self.debug)

        #FTP
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
        self.address = ('', self.filePort)
        self.server = FTPServer(self.address, self.handler)

        # set a limit for connections
        self.server.max_cons = 256
        self.server.max_cons_per_ip = 5

        # Zeroconf
        try:
            self.fileService.publish()
        except Exception as e:
            printError('cannot register DNS service' + str(e))
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


class Canon():
    def __init__(self, parameterFile="", debug=False):
        self.debug = debug
        self.aborted = multiprocessing.Value('b', False, lock=False)
        self.parameterFile = parameterFile

    def do_cancel(self):
        if self.debug:
            print("setting abort flag")
        self.aborted.value = True

    def check_abort(self):
        if self.debug:
            print("check_abort")
        return self.aborted.value

    def reset(self):
        self.aborted.value = False


# Preview class works concurrently using multiprocessing
# Queues are used for communication
class Preview():
    def __init__(self, parameterFile="", initcode="", debug=False):
        self.debug = debug
        self.filename = ""
        self.unitcode = ""
        self.initcode = initcode
        self.canon = Canon(parameterFile=parameterFile, debug=debug)
        self.preview = None
        self.errorCallback = None

        # multiprocessing tools
        self.bindEvent = multiprocessing.Event()
        self.bindCompletedEvent = multiprocessing.Event()
        self.previewEvent = multiprocessing.Event()
        self.previewCompleteEvent = multiprocessing.Event()
        self.shutdownEvent = multiprocessing.Event()
        self.errorEvent = multiprocessing.Event()
        self.isStarted = multiprocessing.Value('b', False, lock=False)
        self.isBound = multiprocessing.Value('b', False, lock=False)
        self.isRunning = multiprocessing.Value('b', False, lock=False)
        self.inqueue = multiprocessing.Queue()  # used to send data to the process
        self.outqueue = multiprocessing.Queue()  # used to get data from the process
        self.process = multiprocessing.Process(target=self.run)
        self.process.start()

    def register_error_callback(self, callback):
        self.errorCallback = callback

    def bind(self, previewUri, statusUri):
        self.inqueue.put((previewUri, statusUri))
        self.bindEvent.set()
        self.bindCompletedEvent.wait()
        return self.outqueue.get()

    def abort(self):
        self.canon.do_cancel()

    def program_open(self, filename):
        if os.path.isfile(filename):
            self.filename = filename
        else:
            raise Exception("file does not exist " + filename)

    def start(self):
        if self.isRunning.value is True:
            raise Exception("Preview already running")

        # start the monitoring daemon
        thread = threading.Thread(target=self.monitoring_thread)
        thread.daemon = True
        thread.start()

    def stop(self):
        self.shutdownEvent.set()
        self.previewCompleteEvent.set()  # in case we are monitoring
        self.process.join()  # make sure to have one process at exit

    def monitoring_thread(self):
        if not self.isBound.value:
            raise Exception('Preview is not bound')

        # put everything on the process queue
        self.inqueue.put((self.filename, self.unitcode, self.initcode))
        # reset canon
        self.canon.reset()
        # release the dragon
        self.previewEvent.set()
        self.previewCompleteEvent.wait()
        # handle error events
        if self.errorEvent.wait(0.1):
            (error, line) = self.outqueue.get()
            if self.errorCallback is not None:
                self.errorCallback(error, line)
            self.errorEvent.clear()

    def run(self):
        import preview  # must be imported in new process to work properly
        self.preview = preview

        self.isStarted.value = True

        # waiting for a bind event
        while not self.bindEvent.wait(timeout=0.1):
            if self.shutdownEvent.is_set():  # in case someone shuts down when we are not bound
                self.isStarted.value = False

        # bind the socket here
        (previewUri, statusUri) = self.inqueue.get()
        (previewUri, statusUri) = self.preview.bind(previewUri, statusUri)
        self.outqueue.put((previewUri, statusUri))
        self.isBound.value = True
        self.bindCompletedEvent.set()
        if self.debug:
            print('Preview socket bound')

        # wait for preview request or shutdown
        # event handshaking is used to synchronize the monitoring thread
        # and the process
        while not self.shutdownEvent.is_set():
            if self.previewEvent.wait(timeout=0.1):
                self.previewCompleteEvent.clear()
                self.isRunning.value = True
                self.do_preview()
                self.previewCompleteEvent.set()
                self.previewEvent.clear()
                self.isRunning.value = False

        if self.debug:
            print('Preview process exited')
        self.isStarted.value = False

    def do_preview(self):
        # get all stuff that is modified at runtime
        (filename, unitcode, initcode) = self.inqueue.get()
        if self.debug:
            print("Preview starting")
            print("Filename: " + filename)
            print("Unitcode: " + unitcode)
            print("Initcode: " + initcode)
        try:
            # here we do all the actual work...
            (result, last_sequence_number) = self.preview.parse(
                filename,
                self.canon,
                unitcode,
                initcode)

            # check if we encountered a error during execution
            if result > self.preview.MIN_ERROR:
                error = " gcode error: %s " % (self.preview.strerror(result))
                line = last_sequence_number - 1
                if self.debug:
                    printError("preview: " + self.filename)
                    printError(error + " on line " + str(line))
                # pass error through queue
                self.outqueue.put((error, str(line)))
                self.errorEvent.set()

        except Exception as e:
            printError("preview exception " + str(e))

        if self.debug:
            print("Preview exiting")


class StatusValues():

    def __init__(self):
        self.io = EmcStatusIo()
        self.config = EmcStatusConfig()
        self.motion = EmcStatusMotion()
        self.task = EmcStatusTask()
        self.interp = EmcStatusInterp()

    def clear(self):
        self.io.Clear()
        self.config.Clear()
        self.motion.Clear()
        self.task.Clear()
        self.interp.Clear()


class LinuxCNCWrapper():

    def __init__(self, context, host='', loopback=False,
                iniFile=None, svcUuid=None,
                pollInterval=None, pingInterval=2, debug=False):
        self.debug = debug
        self.host = host
        self.loopback = loopback
        self.pingInterval = pingInterval
        self.shutdown = threading.Event()
        self.running = False

        # synchronization locks
        self.commandLock = threading.Lock()
        self.statusLock = threading.Lock()
        self.errorLock = threading.Lock()
        self.errorNoteLock = threading.Lock()

        # status
        self.status = StatusValues()
        self.statusTx = StatusValues()
        self.motionSubscribed = False
        self.motionFullUpdate = False
        self.motionFirstrun = True
        self.ioSubscribed = False
        self.ioFullUpdate = False
        self.ioFirstrun = True
        self.taskSubscribed = False
        self.taskFullUpdate = False
        self.taskFirstrun = True
        self.configSubscribed = False
        self.configFullUpdate = False
        self.configFirstrun = True
        self.interpSubscribed = False
        self.interpFullUpdate = False
        self.interpFirstrun = True
        self.statusServiceSubscribed = False

        self.textSubscribed = False
        self.displaySubscribed = False
        self.errorSubscribed = False
        self.errorServiceSubscribed = False
        self.newErrorSubscription = False

        self.linuxcncErrors = []
        self.programExtensions = {}

        # Linuxcnc
        try:
            self.stat = linuxcnc.stat()
            self.command = linuxcnc.command()
            self.error = linuxcnc.error_channel()

            iniFile = iniFile or os.environ.get('INI_FILE_NAME', '/dev/null')
            self.ini = linuxcnc.ini(iniFile)
            self.directory = self.ini.find('DISPLAY', 'PROGRAM_PREFIX') or os.getcwd()
            self.directory = os.path.abspath(os.path.expanduser(self.directory))
            self.pollInterval = float(pollInterval or self.ini.find('DISPLAY', 'CYCLE_TIME') or 0.1)
            self.interpParameterFile = self.ini.find('RS274NGC', 'PARAMETER_FILE') or "linuxcnc.var"
            self.interpParameterFile = os.path.abspath(os.path.expanduser(self.interpParameterFile))
            self.interpInitcode = self.ini.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""

            # setup program extensions
            extensions = self.ini.findall("FILTER", "PROGRAM_EXTENSION")
            for line in extensions:
                splitted = line.split(' ')
                splitted = splitted[0].split(',')
                for extension in splitted:
                    if extension[0] == '.':
                        extension = extension[1:]
                    program = self.ini.find("FILTER", extension) or ""
                    if program is not "":
                        self.programExtensions[extension] = program

            # initialize total line count
            self.totalLines = 0
            # If specified in the ini, try to open the  default file
            openFile = self.ini.find('DISPLAY', 'OPEN_FILE') or ""
            openFile = openFile.strip('"')  # quote signs are allowed
            if openFile != "":
                openFile = os.path.abspath(os.path.expanduser(openFile))
                fileName = os.path.basename(openFile)
                filePath = os.path.join(self.directory, fileName)
                shutil.copy(openFile, filePath)
                if self.debug:
                    print(str("loading default file " + openFile))
                filePath = self.preprocess_program(filePath)
                self.command.mode(linuxcnc.MODE_AUTO)
                self.command.wait_complete()
                self.command.program_open(filePath)

        except linuxcnc.error as detail:
            printError(str(detail))
            sys.exit(1)

        if self.pingInterval > 0:
            self.pingRatio = math.floor(self.pingInterval / self.pollInterval)
        else:
            self.pingRatio = -1
        self.pingCount = 0

        self.rx = Container()          # Used by the command socket
        self.txStatus = Container()    # Status socket - PUB-SUB
        self.txCommand = Container()   # Command socket - ROUTER-DEALER
        self.txError = Container()     # Error socket - PUB-SUB
        self.context = context
        self.baseUri = "tcp://"
        if self.loopback:
            self.baseUri += '127.0.0.1'
        else:
            self.baseUri += '*'
        self.statusSocket = context.socket(zmq.XPUB)
        self.statusSocket.setsockopt(zmq.XPUB_VERBOSE, 1)
        self.statusPort = self.statusSocket.bind_to_random_port(self.baseUri)
        self.statusDsname = self.statusSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.statusDsname = self.statusDsname.replace('0.0.0.0', self.host)
        self.errorSocket = context.socket(zmq.XPUB)
        self.errorSocket.setsockopt(zmq.XPUB_VERBOSE, 1)
        self.errorPort = self.errorSocket.bind_to_random_port(self.baseUri)
        self.errorDsname = self.errorSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.errorDsname = self.errorDsname.replace('0.0.0.0', self.host)
        self.commandSocket = context.socket(zmq.ROUTER)
        self.commandPort = self.commandSocket.bind_to_random_port(self.baseUri)
        self.commandDsname = self.commandSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.commandDsname = self.commandDsname.replace('0.0.0.0', self.host)
        self.preview = Preview(parameterFile=self.interpParameterFile,
                               initcode=self.interpInitcode,
                               debug=self.debug)
        (self.previewDsname, self.previewstatusDsname) = \
            self.preview.bind(self.baseUri + ':*', self.baseUri + ':*')
        self.previewDsname = self.previewDsname.replace('0.0.0.0', self.host)
        self.previewstatusDsname = self.previewstatusDsname.replace('0.0.0.0', self.host)
        self.preview.register_error_callback(self.preview_error)
        self.previewPort = urlparse(self.previewDsname).port
        self.previewstatusPort = urlparse(self.previewstatusDsname).port

        self.statusService = service.Service(type='status',
                                   svcUuid=svcUuid,
                                   dsn=self.statusDsname,
                                   port=self.statusPort,
                                   host=self.host,
                                   loopback=self.loopback,
                                   debug=self.debug)
        self.errorService = service.Service(type='error',
                                   svcUuid=svcUuid,
                                   dsn=self.errorDsname,
                                   port=self.errorPort,
                                   host=self.host,
                                   loopback=self.loopback,
                                   debug=self.debug)
        self.commandService = service.Service(type='command',
                                   svcUuid=svcUuid,
                                   dsn=self.commandDsname,
                                   port=self.commandPort,
                                   host=self.host,
                                   loopback=self.loopback,
                                   debug=self.debug)
        self.previewService = service.Service(type='preview',
                                   svcUuid=svcUuid,
                                   dsn=self.previewDsname,
                                   port=self.previewPort,
                                   host=self.host,
                                   loopback=self.loopback,
                                   debug=self.debug)
        self.previewstatusService = service.Service(type='previewstatus',
                                   svcUuid=svcUuid,
                                   dsn=self.previewstatusDsname,
                                   port=self.previewstatusPort,
                                   host=self.host,
                                   loopback=self.loopback,
                                   debug=self.debug)

        self.publish()

        threading.Thread(target=self.process_sockets).start()
        threading.Thread(target=self.poll).start()
        self.running = True

    def process_sockets(self):
        poll = zmq.Poller()
        poll.register(self.statusSocket, zmq.POLLIN)
        poll.register(self.errorSocket, zmq.POLLIN)
        poll.register(self.commandSocket, zmq.POLLIN)

        while not self.shutdown.is_set():
            s = dict(poll.poll(1000))
            if self.statusSocket in s and s[self.statusSocket] == zmq.POLLIN:
                self.process_status(self.statusSocket)
            if self.errorSocket in s and s[self.errorSocket] == zmq.POLLIN:
                self.process_error(self.errorSocket)
            if self.commandSocket in s and s[self.commandSocket] == zmq.POLLIN:
                self.process_command(self.commandSocket)

        self.unpublish()
        self.running = False
        return

    def publish(self):
        # Zeroconf
        try:
            self.statusService.publish()
            self.errorService.publish()
            self.commandService.publish()
            self.previewService.publish()
            self.previewstatusService.publish()
        except Exception as e:
            printError('cannot register DNS service' + str(e))
            sys.exit(1)

    def unpublish(self):
        self.statusService.unpublish()
        self.errorService.unpublish()
        self.commandService.unpublish()
        self.previewService.unpublish()
        self.previewstatusService.unpublish()

    def stop(self):
        self.shutdown.set()
        self.preview.stop()

    # handle program extensions
    def preprocess_program(self, filePath):
        fileName, extension = os.path.splitext(filePath)
        extension = extension[1:]  # remove dot
        if extension in self.programExtensions:
            program = self.programExtensions[extension]
            newFileName = fileName + '.ngc'
            try:
                outFile = open(newFileName, 'w')
                process = subprocess.Popen([program, filePath], stdout=outFile)
                #subprocess.check_output([program, filePath],
                unused_out, err = process.communicate()
                retcode = process.poll()
                if retcode:
                    raise subprocess.CalledProcessError(retcode, '', output=err)
                outFile.close()
                filePath = newFileName
            except IOError as e:
                self.add_error(str(e))
                return ''
            except subprocess.CalledProcessError as e:
                self.add_error('%s failed: %s' % (program, str(e)))
                return ''
        # get number of lines
        with open(filePath) as f:
            self.totalLines = sum(1 for line in f)
        return filePath

    def notEqual(self, a, b):
        threshold = 0.0001
        return abs(a - b) > threshold

    def zero_position(self):
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

    def check_position(self, oldPosition, newPosition):
        modified = False
        txPosition = Position()

        if self.notEqual(oldPosition.x, newPosition[0]):
            txPosition.x = newPosition[0]
            modified = True
        if self.notEqual(oldPosition.y, newPosition[1]):
            txPosition.y = newPosition[1]
            modified = True
        if self.notEqual(oldPosition.z, newPosition[2]):
            txPosition.z = newPosition[2]
            modified = True
        if self.notEqual(oldPosition.a, newPosition[3]):
            txPosition.a = newPosition[3]
            modified = True
        if self.notEqual(oldPosition.b, newPosition[4]):
            txPosition.b = newPosition[4]
            modified = True
        if self.notEqual(oldPosition.c, newPosition[5]):
            txPosition.c = newPosition[5]
            modified = True
        if self.notEqual(oldPosition.u, newPosition[6]):
            txPosition.u = newPosition[6]
            modified = True
        if self.notEqual(oldPosition.v, newPosition[7]):
            txPosition.v = newPosition[7]
            modified = True
        if self.notEqual(oldPosition.w, newPosition[8]):
            txPosition.w = newPosition[8]
            modified = True

        if modified:
            return True, txPosition
        else:
            del txPosition
            return False, None

    def update_proto_value(self, obj, txObj, prop, value):
        if getattr(obj, prop) != value:
            setattr(obj, prop, value)
            setattr(txObj, prop, value)
            return True
        return False

    def update_proto_float(self, obj, txObj, prop, value):
        if self.notEqual(getattr(obj, prop), value):
            setattr(obj, prop, value)
            setattr(txObj, prop, value)
            return True
        return False

    def update_proto_list(self, obj, txObj, txObjItem, prop, values, default):
        modified = False
        for index, value in enumerate(values):
            txObjItem.Clear()
            objModified = False

            if len(obj) == index:
                obj.add()
                obj[index].index = index
                setattr(obj[index], prop, default)

            objItem = obj[index]
            objModified |= self.update_proto_value(objItem, txObjItem, prop, value)

            if objModified:
                txObjItem.index = index
                txObj.add().CopyFrom(txObjItem)
                modified = True

        return modified

    def update_proto_position(self, obj, txObj, prop, value):
        modified, txPosition = self.check_position(getattr(obj, prop), value)
        if modified:
            getattr(obj, prop).MergeFrom(txPosition)
            getattr(txObj, prop).CopyFrom(txPosition)

        del txPosition
        return modified

    def update_config_value(self, prop, value):
        return self.update_proto_value(self.status.config, self.statusTx.config, prop, value)

    def update_config_float(self, prop, value):
        return self.update_proto_float(self.status.config, self.statusTx.config, prop, value)

    def update_io_value(self, prop, value):
        return self.update_proto_value(self.status.io, self.statusTx.io, prop, value)

    def update_task_value(self, prop, value):
        return self.update_proto_value(self.status.task, self.statusTx.task, prop, value)

    def update_interp_value(self, prop, value):
        return self.update_proto_value(self.status.interp, self.statusTx.interp, prop, value)

    def update_motion_value(self, prop, value):
        return self.update_proto_value(self.status.motion, self.statusTx.motion, prop, value)

    def update_motion_float(self, prop, value):
        return self.update_proto_float(self.status.motion, self.statusTx.motion, prop, value)

    def update_config(self, stat):
        modified = False

        if self.configFirstrun:
            self.status.config.default_acceleration = 0.0
            self.status.config.angular_units = 0.0
            self.status.config.axes = 0
            self.status.config.axis_mask = 0
            self.status.config.cycle_time = 0.0
            self.status.config.debug = 0
            self.status.config.kinematics_type = KINEMATICS_IDENTITY
            self.status.config.linear_units = 0.0
            self.status.config.max_acceleration = 0.0
            self.status.config.max_velocity = 0.0
            self.status.config.program_units = CANON_UNITS_INCHES
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
            self.configFirstrun = False

            extensions = self.ini.findall("FILTER", "PROGRAM_EXTENSION")
            txObjItem = EmcProgramExtension()
            obj = self.status.config.program_extension
            txObj = self.statusTx.config.program_extension
            modified |= self.update_proto_list(obj, txObj, txObjItem,
                                               'extension', extensions, '')
            del txObjItem

            commands = self.ini.findall("DISPLAY", "USER_COMMAND")
            txObjItem = EmcStatusUserCommand()
            obj = self.status.config.user_command
            txObj = self.statusTx.config.user_command
            modified |= self.update_proto_list(obj, txObj, txObjItem,
                                               'command', commands, '')
            del txObjItem

            positionOffset = self.ini.find('DISPLAY', 'POSITION_OFFSET') or 'RELATIVE'
            if positionOffset == 'MACHINE':
                positionOffset = EMC_CONFIG_MACHINE_OFFSET
            else:
                positionOffset = EMC_CONFIG_RELATIVE_OFFSET
            modified |= self.update_config_value('position_offset', positionOffset)

            positionFeedback = self.ini.find('DISPLAY', 'POSITION_FEEDBACK') or 'ACTUAL'
            if positionFeedback == 'COMMANDED':
                positionFeedback = EMC_CONFIG_COMMANDED_FEEDBACK
            else:
                positionFeedback = EMC_CONFIG_ACTUAL_FEEDBACK
            modified |= self.update_config_value('position_feedback', positionFeedback)

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

            value = self.ini.find('DISPLAY', 'INCREMENTS') or '1.0 0.1 0.01 0.001'
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

            timeUnits = str(self.ini.find('DISPLAY', 'TIME_UNITS') or 'min')
            if (timeUnits in ['min', 'minute']):
                timeUnitsConverted = TIME_UNITS_MINUTE
            elif (timeUnits in ['s', 'second']):
                timeUnitsConverted = TIME_UNITS_SECOND
            else:
                timeUnitsConverted = TIME_UNITS_MINUTE
            modified |= self.update_config_value('time_units', timeUnitsConverted)

            modified |= self.update_config_value('remote_path', self.directory)

            name = str(self.ini.find('EMC', 'MACHINE') or '')
            modified |= self.update_config_value('name', name)

        for name in ['axis_mask', 'debug', 'kinematics_type', 'program_units',
                     'axes']:
            modified |= self.update_config_value(name, getattr(stat, name))

        for name in ['cycle_time', 'linear_units', 'angular_units']:
            modified |= self.update_config_float(name, getattr(stat, name))

        modified |= self.update_config_float('default_acceleration', stat.acceleration)
        modified |= self.update_config_float('default_velocity', stat.velocity)

        txAxis = EmcStatusConfigAxis()
        for index, statAxis in enumerate(stat.axis):
            txAxis.Clear()
            axisModified = False

            if index == stat.axes:
                break

            if len(self.status.config.axis) == index:
                self.status.config.axis.add()
                self.status.config.axis[index].index = index
                self.status.config.axis[index].axisType = EMC_AXIS_LINEAR
                self.status.config.axis[index].backlash = 0.0
                self.status.config.axis[index].max_ferror = 0.0
                self.status.config.axis[index].max_position_limit = 0.0
                self.status.config.axis[index].min_ferror = 0.0
                self.status.config.axis[index].min_position_limit = 0.0
                self.status.config.axis[index].units = 0.0
                self.status.config.axis[index].home_sequence = -1
                self.status.config.axis[index].max_velocity = 0.0
                self.status.config.axis[index].max_acceleration = 0.0
                self.status.config.axis[index].increments = ""

                axis = self.status.config.axis[index]
                axisName = 'AXIS_%i' % index
                value = int(self.ini.find(axisName, 'HOME_SEQUENCE') or -1)
                axisModified |= self.update_proto_value(axis, txAxis,
                                                        'home_sequence', value)

                value = float(self.ini.find(axisName, 'MAX_VELOCITY') or 0.0)
                axisModified |= self.update_proto_value(axis, txAxis,
                                                        'max_velocity', value)

                value = float(self.ini.find(axisName, 'MAX_ACCELERATION') or 0.0)
                axisModified |= self.update_proto_value(axis, txAxis,
                                                        'max_acceleration', value)

                value = self.ini.find(axisName, 'INCREMENTS') or ''
                axisModified |= self.update_proto_value(axis, txAxis,
                                                        'increments', value)

            axis = self.status.config.axis[index]
            axisModified |= self.update_proto_value(axis, txAxis, 'axisType', statAxis['axisType'])

            for name in ['backlash', 'max_ferror', 'max_position_limit',
                         'min_ferror', 'min_position_limit', 'units']:
                axisModified |= self.update_proto_float(axis, txAxis, name, statAxis[name])

            if axisModified:
                txAxis.index = index
                self.statusTx.config.axis.add().CopyFrom(txAxis)
                modified = True

        del txAxis

        if self.configFullUpdate:
            self.add_pparams()
            self.send_config(self.status.config, MT_EMCSTAT_FULL_UPDATE)
            self.configFullUpdate = False
        elif modified:
            self.send_config(self.statusTx.config, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_io(self, stat):
        modified = False

        if self.ioFirstrun:
            self.status.io.estop = 0
            self.status.io.flood = 0
            self.status.io.lube = 0
            self.status.io.lube_level = 0
            self.status.io.mist = 0
            self.status.io.pocket_prepped = 0
            self.status.io.tool_in_spindle = 0
            self.status.io.tool_offset.MergeFrom(self.zero_position())
            self.ioFirstrun = False

        for name in ['estop', 'flood', 'lube', 'lube_level', 'mist',
                     'pocket_prepped', 'tool_in_spindle']:
            modified |= self.update_io_value(name, getattr(stat, name))

        modified |= self.update_proto_position(self.status.io, self.statusTx.io,
                                               'tool_offset', stat.tool_offset)

        txToolResult = EmcToolData()
        for index, statToolResult in enumerate(stat.tool_table):
            txToolResult.Clear()
            resultModified = False

            if (statToolResult.id == -1) and (index > 0):  # last tool in table
                break

            if len(self.status.io.tool_table) == index:
                self.status.io.tool_table.add()
                self.status.io.tool_table[index].index = index
                self.status.io.tool_table[index].id = 0
                self.status.io.tool_table[index].xOffset = 0.0
                self.status.io.tool_table[index].yOffset = 0.0
                self.status.io.tool_table[index].zOffset = 0.0
                self.status.io.tool_table[index].aOffset = 0.0
                self.status.io.tool_table[index].bOffset = 0.0
                self.status.io.tool_table[index].cOffset = 0.0
                self.status.io.tool_table[index].uOffset = 0.0
                self.status.io.tool_table[index].vOffset = 0.0
                self.status.io.tool_table[index].wOffset = 0.0
                self.status.io.tool_table[index].diameter = 0.0
                self.status.io.tool_table[index].frontangle = 0.0
                self.status.io.tool_table[index].backangle = 0.0
                self.status.io.tool_table[index].orientation = 0

            toolResult = self.status.io.tool_table[index]

            for name in ['id', 'orientation']:
                value = getattr(statToolResult, name)
                resultModified |= self.update_proto_value(toolResult, txToolResult,
                                                          name, value)
            for name in ['diameter', 'frontangle', 'backangle']:
                value = getattr(statToolResult, name)
                resultModified |= self.update_proto_float(toolResult, txToolResult,
                                                          name, value)

            for axis in ['x', 'y', 'z', 'a', 'b', 'c', 'u', 'v', 'w']:
                value = getattr(statToolResult, axis + 'offset')
                resultModified |= self.update_proto_float(toolResult, txToolResult,
                                                          axis + 'Offset', value)

            if resultModified:
                txToolResult.index = index
                self.statusTx.io.tool_table.add().CopyFrom(txToolResult)
                modified = True
        del txToolResult

        if self.ioFullUpdate:
            self.add_pparams()
            self.send_io(self.status.io, MT_EMCSTAT_FULL_UPDATE)
            self.ioFullUpdate = False
        elif modified:
            self.send_io(self.statusTx.io, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_task(self, stat):
        modified = False

        if self.taskFirstrun:
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
            self.taskFirstrun = False

        for name in ['echo_serial_number', 'exec_state', 'file',
                     'input_timeout', 'optional_stop', 'read_line',
                     'task_mode', 'task_paused', 'task_state']:
            modified |= self.update_task_value(name, getattr(stat, name))

        modified |= self.update_task_value('total_lines', self.totalLines)

        if self.taskFullUpdate:
            self.add_pparams()
            self.send_task(self.status.task, MT_EMCSTAT_FULL_UPDATE)
            self.taskFullUpdate = False
        elif modified:
            self.send_task(self.statusTx.task, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_interp(self, stat):
        modified = False

        if self.interpFirstrun:
            self.status.interp.command = ""
            self.status.interp.interp_state = EMC_TASK_INTERP_IDLE
            self.status.interp.interpreter_errcode = 0
            self.interpFirstrun = False

        for name in ['command', 'interp_state', 'interpreter_errcode']:
            modified |= self.update_interp_value(name, getattr(stat, name))

        txObjItem = EmcStatusGCode()
        obj = self.status.interp.gcodes
        txObj = self.statusTx.interp.gcodes
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.gcodes, 0)
        del txObjItem

        txObjItem = EmcStatusMCode()
        obj = self.status.interp.mcodes
        txObj = self.statusTx.interp.mcodes
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.mcodes, 0)
        del txObjItem

        txObjItem = EmcStatusSetting()
        obj = self.status.interp.settings
        txObj = self.statusTx.interp.settings
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.settings, 0.0)
        del txObjItem

        if self.interpFullUpdate:
            self.add_pparams()
            self.send_interp(self.status.interp, MT_EMCSTAT_FULL_UPDATE)
            self.interpFullUpdate = False
        elif modified:
            self.send_interp(self.statusTx.interp, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_motion(self, stat):
        modified = False

        if self.motionFirstrun:
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
            self.motionFirstrun = False

        for name in ['active_queue', 'adaptive_feed_enabled', 'block_delete',
                     'current_line', 'enabled', 'feed_hold_enabled',
                     'feed_override_enabled', 'g5x_index', 'id', 'inpos',
                     'motion_line', 'motion_type', 'motion_mode', 'paused',
                     'probe_tripped', 'probe_val', 'probing', 'queue',
                     'queue_full', 'spindle_brake', 'spindle_direction',
                     'spindle_enabled', 'spindle_increasing',
                     'spindle_override_enabled', 'state']:
            modified |= self.update_motion_value(name, getattr(stat, name))

        for name in ['current_vel', 'delay_left', 'distance_to_go',
                     'feedrate', 'rotation_xy', 'spindle_speed',
                     'spindlerate', 'max_acceleration', 'max_velocity']:
            modified |= self.update_motion_float(name, getattr(stat, name))

        for name in ['actual_position', 'dtg', 'g5x_offset', 'g92_offset',
                     'joint_actual_position', 'joint_position', 'position',
                     'probed_position']:
            modified |= self.update_proto_position(self.status.motion,
                                                   self.statusTx.motion,
                                                   name, getattr(stat, name))

        txObjItem = EmcStatusAnalogIO()
        obj = self.status.motion.ain
        txObj = self.statusTx.motion.ain
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.ain, 0.0)
        del txObjItem

        txObjItem = EmcStatusAnalogIO()
        obj = self.status.motion.aout
        txObj = self.statusTx.motion.aout
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.aout, 0.0)
        del txObjItem

        txObjItem = EmcStatusDigitalIO()
        obj = self.status.motion.din
        txObj = self.statusTx.motion.din
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.din, False)
        del txObjItem

        txObjItem = EmcStatusDigitalIO()
        obj = self.status.motion.dout
        txObj = self.statusTx.motion.dout
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.dout, False)
        del txObjItem

        txObjItem = EmcStatusLimit()
        obj = self.status.motion.limit
        txObj = self.statusTx.motion.limit
        modified |= self.update_proto_list(obj, txObj, txObjItem,
                                           'value', stat.limit, False)
        del txObjItem

        txAxis = EmcStatusMotionAxis()
        for index, statAxis in enumerate(stat.axis):
            txAxis.Clear()
            axisModified = False

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
            for name in ['enabled', 'fault', 'homed', 'homing',
                         'inpos', 'max_hard_limit', 'max_soft_limit',
                         'min_hard_limit', 'min_soft_limit',
                         'override_limits']:
                axisModified |= self.update_proto_value(axis, txAxis,
                                                        name, statAxis[name])

            for name in ['ferror_current', 'ferror_highmark', 'input',
                         'output', 'velocity']:
                axisModified |= self.update_proto_float(axis, txAxis,
                                                        name, statAxis[name])

            if axisModified:
                txAxis.index = index
                self.statusTx.motion.axis.add().CopyFrom(txAxis)
                modified = True
        del txAxis

        if self.motionFullUpdate:
            self.add_pparams()
            self.send_motion(self.status.motion, MT_EMCSTAT_FULL_UPDATE)
            self.motionFullUpdate = False
        elif modified:
            self.send_motion(self.statusTx.motion, MT_EMCSTAT_INCREMENTAL_UPDATE)

    def update_status(self, stat):
        self.statusTx.clear()
        if (self.ioSubscribed):
            self.update_io(stat)
        if (self.taskSubscribed):
            self.update_task(stat)
        if (self.interpSubscribed):
            self.update_interp(stat)
        if (self.motionSubscribed):
            self.update_motion(stat)
        if (self.configSubscribed):
            self.update_config(stat)

    def update_error(self, error):
        with self.errorNoteLock:
            for linuxcncError in self.linuxcncErrors:
                self.txError.note.append(linuxcncError)
                self.send_error_msg('error', MT_EMC_NML_ERROR)
            self.linuxcncErrors = []

        if not error:
            return

        kind, text = error
        self.txError.note.append(text)

        if (kind == linuxcnc.NML_ERROR):
            if self.errorSubscribed:
                self.send_error_msg('error', MT_EMC_NML_ERROR)
        elif (kind == linuxcnc.OPERATOR_ERROR):
            if self.errorSubscribed:
                self.send_error_msg('error', MT_EMC_OPERATOR_ERROR)
        elif (kind == linuxcnc.NML_TEXT):
            if self.textSubscribed:
                self.send_error_msg('text', MT_EMC_NML_TEXT)
        elif (kind == linuxcnc.OPERATOR_TEXT):
            if self.textSubscribed:
                self.send_error_msg('text', MT_EMC_OPERATOR_TEXT)
        elif (kind == linuxcnc.NML_DISPLAY):
            if self.displaySubscribed:
                self.send_error_msg('display', MT_EMC_NML_DISPLAY)
        elif (kind == linuxcnc.OPERATOR_DISPLAY):
            if self.displaySubscribed:
                self.send_error_msg('display', MT_EMC_OPERATOR_DISPLAY)

    def add_error(self, note):
        with self.errorNoteLock:
            self.linuxcncErrors.append(note)

    def preview_error(self, error, line):
        self.add_error("%s\non line %s" % (error, str(line)))

    def send_config(self, data, type):
        self.txStatus.emc_status_config.MergeFrom(data)
        if self.debug:
            print("sending config message")
        self.send_status_msg('config', type)

    def send_io(self, data, type):
        self.txStatus.emc_status_io.MergeFrom(data)
        if self.debug:
            print("sending io message")
        self.send_status_msg('io', type)

    def send_task(self, data, type):
        self.txStatus.emc_status_task.MergeFrom(data)
        if self.debug:
            print("sending task message")
        self.send_status_msg('task', type)

    def send_motion(self, data, type):
        self.txStatus.emc_status_motion.MergeFrom(data)
        if self.debug:
            print("sending motion message")
        self.send_status_msg('motion', type)

    def send_interp(self, data, type):
        self.txStatus.emc_status_interp.MergeFrom(data)
        if self.debug:
            print("sending interp message")
        self.send_status_msg('interp', type)

    def send_status_msg(self, topic, type):
        with self.statusLock:
            self.txStatus.type = type
            txBuffer = self.txStatus.SerializeToString()
            self.statusSocket.send_multipart([topic, txBuffer], zmq.NOBLOCK)
            self.txStatus.Clear()

    def send_error_msg(self, topic, type):
        with self.errorLock:
            self.txError.type = type
            txBuffer = self.txError.SerializeToString()
            self.errorSocket.send_multipart([topic, txBuffer], zmq.NOBLOCK)
            self.txError.Clear()

    def send_command_msg(self, identity, type):
        with self.commandLock:
            self.txCommand.type = type
            txBuffer = self.txCommand.SerializeToString()
            self.commandSocket.send_multipart([identity, txBuffer], zmq.NOBLOCK)
            self.txCommand.Clear()

    def add_pparams(self):
        parameters = ProtocolParameters()
        parameters.keepalive_timer = int(self.pingInterval * 1000.0)
        self.txStatus.pparams.MergeFrom(parameters)

    def poll(self):
        while not self.shutdown.is_set():
            try:
                if (self.statusServiceSubscribed):
                    self.stat.poll()
                    self.update_status(self.stat)
                    if (self.pingCount == self.pingRatio):
                        self.ping_status()

                if (self.errorServiceSubscribed):
                    error = self.error.poll()
                    self.update_error(error)
                    if (self.pingCount == self.pingRatio):
                        self.ping_error()

            except linuxcnc.error as detail:
                printError(str(detail))
                self.stop()

            if (self.pingCount == self.pingRatio):
                self.pingCount = 0
            else:
                self.pingCount += 1
            time.sleep(self.pollInterval)

        self.running = False
        return

    def ping_status(self):
        if (self.ioSubscribed):
            self.send_status_msg('io', MT_PING)
        if (self.taskSubscribed):
            self.send_status_msg('task', MT_PING)
        if (self.interpSubscribed):
            self.send_status_msg('interp', MT_PING)
        if (self.motionSubscribed):
            self.send_status_msg('motion', MT_PING)
        if (self.configSubscribed):
            self.send_status_msg('config', MT_PING)

    def ping_error(self):
        if self.newErrorSubscription:        # not very clear
            self.add_pparams()
            self.newErrorSubscription = False

        if (self.errorSubscribed):
            self.send_error_msg('error', MT_PING)
        if (self.textSubscribed):
            self.send_error_msg('text', MT_PING)
        if (self.displaySubscribed):
            self.send_error_msg('display', MT_PING)

    def process_status(self, socket):
        try:
            rc = socket.recv()
            subscription = rc[1:]
            status = (rc[0] == "\x01")

            if subscription == 'motion':
                self.motionSubscribed = status
                self.motionFullUpdate = status
            elif subscription == 'task':
                self.taskSubscribed = status
                self.taskFullUpdate = status
            elif subscription == 'io':
                self.ioSubscribed = status
                self.ioFullUpdate = status
            elif subscription == 'config':
                self.configSubscribed = status
                self.configFullUpdate = status
            elif subscription == 'interp':
                self.interpSubscribed = status
                self.interpFullUpdate = status

            self.statusServiceSubscribed = self.motionSubscribed \
            or self.taskSubscribed \
            or self.ioSubscribed \
            or self.configSubscribed \
            or self.interpSubscribed

            if self.debug:
                print(("process status called " + subscription + ' ' + str(status)))
                print(("status service subscribed: " + str(self.statusServiceSubscribed)))

        except zmq.ZMQError as e:
            printError('ZMQ error: ' + str(e))

    def process_error(self, socket):
        try:
            rc = socket.recv()
            subscription = rc[1:]
            status = (rc[0] == "\x01")

            if subscription == 'error':
                self.newErrorSubscription = status
                self.errorSubscribed = status
            elif subscription == 'text':
                self.newErrorSubscription = status
                self.textSubscribed = status
            elif subscription == 'display':
                self.newErrorSubscription = status
                self.displaySubscribed = status

            self.errorServiceSubscribed = self.errorSubscribed \
            or self.textSubscribed \
            or self.displaySubscribed

            if self.debug:
                print(("process error called " + subscription + ' ' + str(status)))
                print(("error service subscribed: " + str(self.errorServiceSubscribed)))

        except zmq.ZMQError as e:
            printError('ZMQ error: ' + str(e))

    def get_active_gcodes(self):
        rawGcodes = self.stat.gcodes
        gcodes = []
        for rawGCode in rawGcodes:
            if rawGCode > -1:
                gcodes.append('G' + str(rawGCode / 10.0))
        return ' '.join(gcodes)

    def send_command_wrong_params(self, identity):
        self.txCommand.note.append("wrong parameters")
        self.send_command_msg(identity, MT_ERROR)

    def command_completion_process(self, event):
        self.command.wait_complete()  # wait for emcmodule
        event.set()  # inform the listening thread

    def command_completion_thread(self, identity, ticket):
        event = multiprocessing.Event()
        # wait in separate process to prevent GIL from causing problems
        multiprocessing.Process(target=self.command_completion_process,
                                args=(event,)).start()
        # wait until the command is completed
        event.wait()
        self.txCommand.reply_ticket = ticket
        self.send_command_msg(identity, MT_EMCCMD_COMPLETED)

        if self.debug:
            print('command #%i from %s completed' % (ticket, identity))

    def wait_complete(self, identity, ticket):
        self.txCommand.reply_ticket = ticket
        self.send_command_msg(identity, MT_EMCCMD_EXECUTED)

        if self.debug:
            print('waiting for command #%ifrom %s to complete' % (ticket, identity))

        # kick off the monitoring thread
        threading.Thread(target=self.command_completion_thread,
                         args=(identity, ticket, )).start()

    def process_command(self, socket):
        (identity, message) = socket.recv_multipart()
        self.rx.ParseFromString(message)

        if self.debug:
            print("process command called, id: %s" % identity)

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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('line_number') \
                and self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        lineNumber = self.rx.emc_command_params.line_number
                        self.command.auto(linuxcnc.AUTO_RUN, lineNumber)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                    elif self.rx.interp_name == 'preview':
                        self.preview.unitcode = "G%d" % (20 + (self.stat.linear_units == 1))
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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('debug_level'):
                    debugLevel = self.rx.emc_command_params.debug_level
                    self.command.debug(debugLevel)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_SCALE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('scale'):
                    feedrate = self.rx.emc_command_params.scale
                    self.command.feedrate(feedrate)
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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.home(axis)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_ABORT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.jog(linuxcnc.JOG_STOP, axis)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_JOG:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('velocity'):
                    axis = self.rx.emc_command_params.index
                    velocity = self.rx.emc_command_params.velocity
                    self.command.jog(linuxcnc.JOG_CONTINUOUS, axis, velocity)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_INCR_JOG:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('velocity') \
                and self.rx.emc_command_params.HasField('distance'):
                    axis = self.rx.emc_command_params.index
                    velocity = self.rx.emc_command_params.velocity
                    distance = self.rx.emc_command_params.distance
                    self.command.jog(linuxcnc.JOG_INCREMENT, axis, velocity, distance)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TOOL_LOAD_TOOL_TABLE:
                self.command.load_tool_table()
                if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)

            elif self.rx.type == MT_EMC_TRAJ_SET_MAX_VELOCITY:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('velocity'):
                    velocity = self.rx.emc_command_params.velocity
                    self.command.maxvel(velocity)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_EXECUTE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('command') \
                and self.rx.HasField('interp_name'):
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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('task_mode') \
                and self.rx.HasField('interp_name'):
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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('path') \
                and self.rx.HasField('interp_name'):
                    fileName = self.rx.emc_command_params.path
                    fileName = self.preprocess_program(fileName)
                    if fileName is not '':
                        if self.rx.interp_name == 'execute':
                            self.command.program_open(fileName)
                            if self.rx.HasField('ticket'):
                                self.wait_complete(identity, self.rx.ticket)
                        elif self.rx.interp_name == 'preview':
                            self.preview.program_open(fileName)
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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    adaptiveFeed = self.rx.emc_command_params.enable
                    self.command.set_adaptive_feed(adaptiveFeed)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identit, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_MOTION_SET_AOUT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('value'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_analog_output(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_SET_BLOCK_DELETE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    blockDelete = self.rx.emc_command_params.enable
                    self.command.set_block_delete(blockDelete)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_MOTION_SET_DOUT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('enable'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.enable
                    self.command.set_digital_output(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_FH_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    feedHold = self.rx.emc_command_params.enable
                    self.command.set_feed_hold(feedHold)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_FO_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    feedOverride = self.rx.emc_command_params.enable
                    self.command.set_feed_override(feedOverride)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_SET_MAX_POSITION_LIMIT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('value'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_max_limit(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_SET_MIN_POSITION_LIMIT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('value'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_min_limit(axis, value)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_PLAN_SET_OPTIONAL_STOP:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    optionalStop = self.rx.emc_command_params.enable
                    self.command.set_optional_stop(optionalStop)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_SO_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    spindleOverride = self.rx.emc_command_params.enable
                    self.command.set_spindle_override(spindleOverride)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_SPINDLE_ON:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('velocity'):
                    speed = self.rx.emc_command_params.velocity
                    direction = linuxcnc.SPINDLE_FORWARD    # always forwward, speed can be signed
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
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('scale'):
                    scale = self.rx.emc_command_params.scale
                    self.command.spindleoverride(scale)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TASK_SET_STATE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('task_state') \
                and self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.state(self.rx.emc_command_params.task_state)
                        if self.rx.HasField('ticket'):
                            self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_TELEOP_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    teleopEnable = self.rx.emc_command_params.enable
                    self.command.teleop_enable(teleopEnable)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_TELEOP_VECTOR:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('pose') \
                and self.rx.emc_command_params.pose.HasField('a') \
                and self.rx.emc_command_params.pose.HasField('b') \
                and self.rx.emc_command_params.pose.HasField('c'):
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

            elif self.rx.type == MT_EMC_TOOL_SET_OFFSET:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('tool_data') \
                and self.rx.emc_command_params.tool_data.index \
                and self.rx.emc_command_params.tool_data.zOffset \
                and self.rx.emc_command_params.tool_data.xOffset \
                and self.rx.emc_command_params.tool_data.diameter \
                and self.rx.emc_command_params.tool_data.frontangle \
                and self.rx.emc_command_params.tool_data.backangle \
                and self.rx.emc_command_params.tool_data.orientation:
                    toolno = self.rx.emc_command_params.tool_data.index
                    z_offset = self.rx.emc_command_params.tool_data.zOffset
                    x_offset = self.rx.emc_command_params.tool_data.xOffset
                    diameter = self.rx.emc_command_params.tool_data.diameter
                    frontangle = self.rx.emc_command_params.tool_data.frontangle
                    backangle = self.rx.emc_command_params.tool_data.backangle
                    orientation = self.rx.emc_command_params.tool_data.orientation
                    self.command.tool_offset(toolno, z_offset, x_offset, diameter,
                        frontangle, backangle, orientation)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_TRAJ_SET_MODE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('traj_mode'):
                    self.command.traj_mode(self.rx.emc_command_params.traj_mode)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            elif self.rx.type == MT_EMC_AXIS_UNHOME:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.unhome(axis)
                    if self.rx.HasField('ticket'):
                        self.wait_complete(identity, self.rx.ticket)
                else:
                    self.send_command_wrong_params(identity)

            else:
                self.txCommand.note.append("unknown command")
                self.send_command_msg(identity, MT_ERROR)

        except linuxcnc.error as detail:
            self.add_error(detail)
        except UnicodeEncodeError:
            self.add_error("Please use only ASCII characters")
        except Exception as e:
            printError('uncaught exception ' + str(e))
            self.add_error(str(e))


shutdown = False


def _exitHandler(signum, frame):
    del signum
    del frame
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
    parser = argparse.ArgumentParser(description='Mkwrapper is wrapper around the linuxcnc python module as temporary workaround for Machinetalk based user interfaces')
    parser.add_argument('-ini', help='INI file', default=None)
    parser.add_argument('-d', '--debug', help='Enable debug mode', action='store_true')

    args = parser.parse_args()

    debug = args.debug
    iniFile = args.ini

    mkconfig = config.Config()
    mkini = os.getenv("MACHINEKIT_INI")
    if mkini is None:
        mkini = mkconfig.MACHINEKIT_INI
    if not os.path.isfile(mkini):
        sys.stderr.write("MACHINEKIT_INI " + mkini + " does not exist\n")
        sys.exit(1)

    mki = ConfigParser.ConfigParser()
    mki.read(mkini)
    mkUuid = mki.get("MACHINEKIT", "MKUUID")
    remote = mki.getint("MACHINEKIT", "REMOTE")

    if remote == 0:
        print("Remote communication is deactivated, mkwrapper will use the loopback interfaces")
        print(("set REMOTE in " + mkini + " to 1 to enable remote communication"))

    if debug:
        print("announcing mkwrapper")

    context = zmq.Context()
    context.linger = 0

    register_exit_handler()

    fileService = None
    mkwrapper = None
    try:
        hostname = '%(fqdn)s'  # replaced by service announcement
        fileService = FileService(iniFile=iniFile,
                                  svcUuid=mkUuid,
                                  host=hostname,
                                  loopback=(not remote),
                                  debug=debug)
        fileService.start()

        mkwrapper = LinuxCNCWrapper(context,
                                    host=hostname,
                                    loopback=(not remote),
                                    iniFile=iniFile,
                                    svcUuid=mkUuid,
                                    debug=debug)

        while fileService.running and mkwrapper.running and not check_exit():
            time.sleep(1)
    except Exception as e:
        printError("uncaught exception: " + str(e))

    print("stopping threads")
    if fileService is not None:
        fileService.stop()
    if mkwrapper is not None:
        mkwrapper.stop()

    # wait for all threads to terminate
    while threading.active_count() > 2:  # one thread for every process is left
        time.sleep(0.1)

    print("threads stopped")
    sys.exit(0)

if __name__ == "__main__":
    main()
