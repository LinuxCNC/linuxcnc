#!/usr/bin/python
import os
import sys
from stat import *
import zmq
import netifaces
import threading
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
import preview
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


# Handle uploaded files to delete them after program exit
uploadedFiles = set()


def addUploadedFile(file):
    global uploadedFiles
    uploadedFiles.add(file)


def clearUploadedFiles():
    global uploadedFiles
    for uploadedFile in uploadedFiles:
        os.remove(uploadedFile)
    uploadedFiles = set()


class CustomFTPHandler(FTPHandler):

    def on_file_received(self, file):
        # do something when a file has been received
        addUploadedFile(file)

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
        self.authorizer.add_anonymous(self.directory, perm="lradw")

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

    def __del__(self):
        clearUploadedFiles()

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
        self.aborted = False
        self.parameterFile = parameterFile

    def do_cancel(self):
        if self.debug:
            print("setting abort flag")
        self.aborted = True

    def check_abort(self):
        if self.debug:
            print("check_abort")
        return self.aborted

    def reset(self):
        self.aborted = False


class Preview():
    def __init__(self, parameterFile="", debug=False):
        self.filename = ""
        self.unitcode = ""
        self.initcode = ""
        self.canon = Canon(parameterFile=parameterFile, debug=debug)
        self.debug = debug
        self.isRunning = False
        self.errorCallback = None

    def register_error_callback(self, callback):
        self.errorCallback = callback

    def bind(self, previewUri, statusUri):
        return preview.bind(previewUri, statusUri)

    def abort(self):
        self.canon.do_cancel()

    def program_open(self, filename):
        if os.path.isfile(filename):
            self.filename = filename
        else:
            raise Exception("file does not exist " + filename)

    def start(self):
        if self.isRunning:
            raise Exception("Preview already running")

        self.canon.reset()
        thread = threading.Thread(target=self.run)
        thread.daemon = True
        thread.start()

    def run(self):
        self.isRunning = True
        if self.debug:
            print("Preview starting")
            print("Filename: " + self.filename)
            print("Unitcode: " + self.unitcode)
            print("Initcode: " + self.initcode)
        try:
            result, last_sequence_number = preview.parse(self.filename,
                                                         self.canon,
                                                         self.unitcode,
                                                         self.initcode)

            if result > preview.MIN_ERROR:
                error = " gcode error: %s " % (preview.strerror(result))
                line = last_sequence_number - 1
                if self.debug:
                    printError("preview: " + self.filename)
                    printError(error + " on line " + str(line))
                if self.errorCallback is not None:

                    self.errorCallback(error, line)

        except Exception as e:
            printError("preview exception" + str(e))

        if self.debug:
            print("Preview exiting")
        self.isRunning = False


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

    def preview_error(self, error, line):
        self.linuxcncErrors.append(error + "\non line " + str(line))

    def __init__(self, context, host='', loopback=False,
                iniFile=None, svcUuid=None,
                pollInterval=None, pingInterval=2, debug=False):
        self.debug = debug
        self.host = host
        self.loopback = loopback
        self.pingInterval = pingInterval
        self.shutdown = threading.Event()
        self.running = False

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
            self.interpParameterFile = self.ini.find('RS274NGC', 'PARAMETER_FILE') or ""
            self.interpParameterFile = os.path.abspath(os.path.expanduser(self.interpParameterFile))
            
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
        self.commandSocket = context.socket(zmq.DEALER)
        self.commandPort = self.commandSocket.bind_to_random_port(self.baseUri)
        self.commandDsname = self.commandSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.commandDsname = self.commandDsname.replace('0.0.0.0', self.host)
        self.preview = Preview(parameterFile=self.interpParameterFile,
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
            if self.statusSocket in s:
                self.process_status(self.statusSocket)
            if self.errorSocket in s:
                self.process_error(self.errorSocket)
            if self.commandSocket in s:
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
                return newFileName
            except IOError as e:
                self.linuxcncErrors.append(str(e))
                return ''
            except subprocess.CalledProcessError as e:
                self.linuxcncErrors.append(e.output)
                return ''
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
            txExtension = EmcProgramExtension()
            for index, extension in enumerate(extensions):
                txExtension.Clear()
                extensionModified = False

                if len(self.status.config.program_extension) == index:
                    self.status.config.program_extension.add()
                    self.status.config.program_extension[index].index = index
                    self.status.config.program_extension[index].extension = ""

                if self.status.config.program_extension[index].extension != extension:
                    self.status.config.program_extension[index].extension = extension
                    txExtension.extension = extension
                    extensionModified = True

                if extensionModified:
                    txExtension.index = index
                    self.statusTx.config.program_extension.add().CopyFrom(txExtension)
                    modified = True
            del txExtension

            positionOffset = self.ini.find('DISPLAY', 'POSITION_OFFSET') or 'RELATIVE'
            if positionOffset == 'MACHINE':
                positionOffset = EMC_CONFIG_MACHINE_OFFSET
            else:
                positionOffset = EMC_CONFIG_RELATIVE_OFFSET
            if (self.status.config.position_offset != positionOffset):
                self.status.config.position_offset = positionOffset
                self.statusTx.config.position_offset = positionOffset
                modified = True

            positionFeedback = self.ini.find('DISPLAY', 'POSITION_OFFSET') or 'ACTUAL'
            if positionFeedback == 'COMMANDED':
                positionFeedback = EMC_CONFIG_COMMANDED_FEEDBACK
            else:
                positionFeedback = EMC_CONFIG_ACTUAL_FEEDBACK
            if (self.status.config.position_feedback != positionFeedback):
                self.status.config.position_feedback = positionFeedback
                self.statusTx.config.position_feedback = positionFeedback
                modified = True

            maxFeedOverride = float(self.ini.find('DISPLAY', 'MAX_FEED_OVERRIDE') or 1.2)
            if (self.status.config.max_feed_override != maxFeedOverride):
                self.status.config.max_feed_override = maxFeedOverride
                self.statusTx.config.max_feed_override = maxFeedOverride
                modified = True

            minFeedOverride = float(self.ini.find('DISPLAY', 'MIN_FEED_OVERRIDE') or 0.5)
            if (self.status.config.min_feed_override != minFeedOverride):
                self.status.config.min_feed_override = minFeedOverride
                self.statusTx.config.min_feed_override = minFeedOverride
                modified = True

            maxSpindleOverride = float(self.ini.find('DISPLAY', 'MAX_SPINDLE_OVERRIDE') or 1.0)
            if (self.status.config.max_spindle_override != maxSpindleOverride):
                self.status.config.max_spindle_override = maxSpindleOverride
                self.statusTx.config.max_spindle_override = maxSpindleOverride
                modified = True

            minSpindleOverride = float(self.ini.find('DISPLAY', 'MIN_SPINDLE_OVERRIDE') or 0.5)
            if (self.status.config.min_spindle_override != minSpindleOverride):
                self.status.config.min_spindle_override = minSpindleOverride
                self.statusTx.config.min_spindle_override = minSpindleOverride
                modified = True

            defaultSpindleSpeed = float(self.ini.find('DISPLAY', 'DEFAULT_SPINDLE_SPEED') or 1)
            if (self.status.config.default_spindle_speed != defaultSpindleSpeed):
                self.status.config.default_spindle_speed = defaultSpindleSpeed
                self.statusTx.config.default_spindle_speed = defaultSpindleSpeed
                modified = True

            defaultLinearVelocity = float(self.ini.find('DISPLAY', 'DEFAULT_LINEAR_VELOCITY') or 0.25)
            if (self.status.config.default_linear_velocity != defaultLinearVelocity):
                self.status.config.default_linear_velocity = defaultLinearVelocity
                self.statusTx.config.default_linear_velocity = defaultLinearVelocity
                modified = True

            minVelocity = float(self.ini.find('DISPLAY', 'MIN_VELOCITY') or 0.01)
            if (self.status.config.min_velocity != minVelocity):
                self.status.config.min_velocity = minVelocity
                self.statusTx.config.min_velocity = minVelocity
                modified = True

            maxLinearVelocity = float(self.ini.find('DISPLAY', 'MAX_LINEAR_VELOCITY') or 1.00)
            if (self.status.config.max_linear_velocity != maxLinearVelocity):
                self.status.config.max_linear_velocity = maxLinearVelocity
                self.statusTx.config.max_linear_velocity = maxLinearVelocity
                modified = True

            minLinearVelocity = float(self.ini.find('DISPLAY', 'MIN_LINEAR_VELOCITY') or 0.01)
            if (self.status.config.min_linear_velocity != minLinearVelocity):
                self.status.config.min_linear_velocity = minLinearVelocity
                self.statusTx.config.min_linear_velocity = minLinearVelocity
                modified = True

            defaultAngularVelocity = float(self.ini.find('DISPLAY', 'DEFAULT_ANGULAR_VELOCITY') or 0.25)
            if (self.status.config.default_angular_velocity != defaultAngularVelocity):
                self.status.config.default_angular_velocity = defaultAngularVelocity
                self.statusTx.config.default_angular_velocity = defaultAngularVelocity
                modified = True

            maxAngularVelocity = float(self.ini.find('DISPLAY', 'MAX_ANGULAR_VELOCITY') or 1.00)
            if (self.status.config.max_angular_velocity != maxAngularVelocity):
                self.status.config.max_angular_velocity = maxAngularVelocity
                self.statusTx.config.max_angular_velocity = maxAngularVelocity
                modified = True

            minAngularVelocity = float(self.ini.find('DISPLAY', 'MIN_ANGULAR_VELOCITY') or 0.01)
            if (self.status.config.min_angular_velocity != minAngularVelocity):
                self.status.config.min_angular_velocity = minAngularVelocity
                self.statusTx.config.min_angular_velocity = minAngularVelocity
                modified = True

            increments = self.ini.find('DISPLAY', 'INCREMENTS') or ''
            if (self.status.config.increments != increments):
                self.status.config.increments = increments
                self.statusTx.config.increments = increments
                modified = True

            grids = self.ini.find('DISPLAY', 'GRIDS') or ''
            if (self.status.config.grids != grids):
                self.status.config.grids = grids
                self.statusTx.config.grids = grids
                modified = True

            lathe = bool(self.ini.find('DISPLAY', 'LATHE') or False)
            if (self.status.config.lathe != lathe):
                self.status.config.lathe = lathe
                self.statusTx.config.lathe = lathe
                modified = True

            geometry = self.ini.find('DISPLAY', 'GEOMETRY') or ''
            if (self.status.config.geometry != geometry):
                self.status.config.geometry = geometry
                self.statusTx.config.geometry = geometry
                modified = True

            arcdivision = int(self.ini.find('DISPLAY', 'ARCDIVISION') or 64)
            if (self.status.config.arcdivision != arcdivision):
                self.status.config.arcdivision = arcdivision
                self.statusTx.config.arcdivision = arcdivision
                modified = True

            noForceHoming = bool(self.ini.find('TRAJ', 'NO_FORCE_HOMING') or False)
            if (self.status.config.no_force_homing != noForceHoming):
                self.status.config.no_force_homing = noForceHoming
                self.statusTx.config.no_force_homing = noForceHoming
                modified = True

            maxVelocity = float(self.ini.find('TRAJ', 'MAX_VELOCITY') or 5.0)
            if (self.status.config.max_velocity != maxVelocity):
                self.status.config.max_velocity = maxVelocity
                self.statusTx.config.max_velocity = maxVelocity
                modified = True

            maxAcceleration = float(self.ini.find('TRAJ', 'MAX_ACCELERATION') or 20.0)
            if (self.status.config.max_acceleration != maxAcceleration):
                self.status.config.max_acceleration = maxAcceleration
                self.statusTx.config.max_acceleration = maxAcceleration
                modified = True

            timeUnits = str(self.ini.find('DISPLAY', 'TIME_UNITS') or 'min')
            if (timeUnits in ['min', 'minute']):
                timeUnitsConverted = TIME_UNITS_MINUTE
            elif (timeUnits in ['s', 'second']):
                timeUnitsConverted = TIME_UNITS_SECOND
            else:
                timeUnitsConverted = TIME_UNITS_MINUTE
            if (self.status.config.time_units != timeUnitsConverted):
                self.status.config.time_units = timeUnitsConverted
                self.statusTx.config.time_units = timeUnitsConverted
                modified = True

            if (self.status.config.remote_path != self.directory):
                self.status.config.remote_path = self.directory
                self.statusTx.config.remote_path = self.directory
                modified = True
           
            name = str(self.ini.find('EMC', 'MACHINE') or '')
            if (self.status.config.name != name):
                self.status.config.name = name
                self.statusTx.config.name = name
                modified = True

        if self.notEqual(self.status.config.default_acceleration, stat.acceleration):
            self.status.config.default_acceleration = stat.acceleration
            self.statusTx.config.default_acceleration = stat.acceleration
            modified = True

        if self.notEqual(self.status.config.angular_units, stat.angular_units):
            self.status.config.angular_units = stat.angular_units
            self.statusTx.config.angular_units = stat.angular_units
            modified = True

        if (self.status.config.axes != stat.axes):
            self.status.config.axes = stat.axes
            self.statusTx.config.axes = stat.axes
            modified = True

        txAxis = EmcStatusConfigAxis()
        for index, axis in enumerate(stat.axis):
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

            if self.status.config.axis[index].axisType != axis['axisType']:
                self.status.config.axis[index].axisType = axis['axisType']
                txAxis.axisType = axis['axisType']
                axisModified = True

            if self.notEqual(self.status.config.axis[index].backlash, axis['backlash']):
                self.status.config.axis[index].backlash = axis['backlash']
                txAxis.backlash = axis['backlash']
                axisModified = True

            if self.notEqual(self.status.config.axis[index].max_ferror, axis['max_ferror']):
                self.status.config.axis[index].max_ferror = axis['max_ferror']
                txAxis.max_ferror = axis['max_ferror']
                axisModified = True

            if self.notEqual(self.status.config.axis[index].max_position_limit, axis['max_position_limit']):
                self.status.config.axis[index].max_position_limit = axis['max_position_limit']
                txAxis.max_position_limit = axis['max_position_limit']
                axisModified = True

            if self.notEqual(self.status.config.axis[index].min_ferror, axis['min_ferror']):
                self.status.config.axis[index].min_ferror = axis['min_ferror']
                txAxis.min_ferror = axis['min_ferror']
                axisModified = True

            if self.notEqual(self.status.config.axis[index].min_position_limit, axis['min_position_limit']):
                self.status.config.axis[index].min_position_limit = axis['min_position_limit']
                txAxis.min_position_limit = axis['min_position_limit']
                axisModified = True

            if self.notEqual(self.status.config.axis[index].units, axis['units']):
                self.status.config.axis[index].units = axis['units']
                txAxis.units = axis['units']
                axisModified = True

            homeSequence = int(self.ini.find('AXIS_' + str(index), 'HOME_SEQUENCE') or -1)
            if (self.status.config.axis[index].home_sequence != homeSequence):
                self.status.config.axis[index].home_sequence = homeSequence
                txAxis.home_sequence = homeSequence
                modified = True

            if axisModified:
                txAxis.index = index
                self.statusTx.config.axis.add().CopyFrom(txAxis)
                modified = True

        del txAxis

        if (self.status.config.axis_mask != stat.axis_mask):
            self.status.config.axis_mask = stat.axis_mask
            self.statusTx.config.axis_mask = stat.axis_mask
            modified = True

        if self.notEqual(self.status.config.cycle_time, stat.cycle_time):
            self.status.config.cycle_time = stat.cycle_time
            self.statusTx.config.cycle_time = stat.cycle_time
            modified = True

        if (self.status.config.debug != stat.debug):
            self.status.config.debug = stat.debug
            self.statusTx.config.debug = stat.debug
            modified = True

        if (self.status.config.kinematics_type != stat.kinematics_type):
            self.status.config.kinematics_type = stat.kinematics_type
            self.statusTx.config.kinematics_type = stat.kinematics_type
            modified = True

        if self.notEqual(self.status.config.linear_units, stat.linear_units):
            self.status.config.linear_units = stat.linear_units
            self.statusTx.config.linear_units = stat.linear_units
            modified = True

        if (self.status.config.program_units != stat.program_units):
            self.status.config.program_units = stat.program_units
            self.statusTx.config.program_units = stat.program_units
            modified = True

        if self.notEqual(self.status.config.default_velocity, stat.velocity):
            self.status.config.default_velocity = stat.velocity
            self.statusTx.config.default_velocity = stat.velocity
            modified = True

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

        if (self.status.io.estop != stat.estop):
            self.status.io.estop = stat.estop
            self.statusTx.io.estop = stat.estop
            modified = True

        if (self.status.io.flood != stat.flood):
            self.status.io.flood = stat.flood
            self.statusTx.io.flood = stat.flood
            modified = True

        if (self.status.io.lube != stat.lube):
            self.status.io.lube = stat.lube
            self.statusTx.io.lube = stat.lube
            modified = True

        if (self.status.io.lube_level != stat.lube_level):
            self.status.io.lube_level = stat.lube_level
            self.statusTx.io.lube_level = stat.lube_level
            modified = True

        if (self.status.io.mist != stat.mist):
            self.status.io.mist = stat.mist
            self.statusTx.io.mist = stat.mist
            modified = True

        if (self.status.io.pocket_prepped != stat.pocket_prepped):
            self.status.io.pocket_prepped = stat.pocket_prepped
            self.statusTx.io.pocket_prepped = stat.pocket_prepped
            modified = True

        if (self.status.io.tool_in_spindle != stat.tool_in_spindle):
            self.status.io.tool_in_spindle = stat.tool_in_spindle
            self.statusTx.io.tool_in_spindle = stat.tool_in_spindle
            modified = True

        positionModified = False
        txPosition = None
        positionModified, txPosition = self.check_position(self.status.io.tool_offset, stat.tool_offset)
        if positionModified:
            self.status.io.tool_offset.MergeFrom(txPosition)
            self.statusTx.io.tool_offset.CopyFrom(txPosition)
            modified = True
        del txPosition

        txToolResult = EmcToolData()
        for index, toolResult in enumerate(stat.tool_table):
            txToolResult.Clear()
            toolResultModified = False

            if (toolResult.id == -1) and (index > 0):  # last tool in table
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

            if self.status.io.tool_table[index].id != toolResult.id:
                self.status.io.tool_table[index].id = toolResult.id
                txToolResult.id = toolResult.id
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].xOffset, toolResult.xoffset):
                self.status.io.tool_table[index].xOffset = toolResult.xoffset
                txToolResult.xOffset = toolResult.xoffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].yOffset, toolResult.yoffset):
                self.status.io.tool_table[index].yOffset = toolResult.yoffset
                txToolResult.yOffset = toolResult.yoffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].zOffset, toolResult.zoffset):
                self.status.io.tool_table[index].zOffset = toolResult.zoffset
                txToolResult.zOffset = toolResult.zoffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].aOffset, toolResult.aoffset):
                self.status.io.tool_table[index].aOffset = toolResult.aoffset
                txToolResult.aOffset = toolResult.aoffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].bOffset, toolResult.boffset):
                self.status.io.tool_table[index].bOffset = toolResult.boffset
                txToolResult.bOffset = toolResult.boffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].cOffset, toolResult.coffset):
                self.status.io.tool_table[index].cOffset = toolResult.coffset
                txToolResult.cOffset = toolResult.coffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].uOffset, toolResult.uoffset):
                self.status.io.tool_table[index].uOffset = toolResult.uoffset
                txToolResult.uOffset = toolResult.uoffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].vOffset, toolResult.voffset):
                self.status.io.tool_table[index].vOffset = toolResult.voffset
                txToolResult.vOffset = toolResult.voffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].wOffset, toolResult.woffset):
                self.status.io.tool_table[index].wOffset = toolResult.woffset
                txToolResult.wOffset = toolResult.woffset
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].diameter, toolResult.diameter):
                self.status.io.tool_table[index].diameter = toolResult.diameter
                txToolResult.diameter = toolResult.diameter
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].frontangle, toolResult.frontangle):
                self.status.io.tool_table[index].frontangle = toolResult.frontangle
                txToolResult.frontangle = toolResult.frontangle
                toolResultModified = True

            if self.notEqual(self.status.io.tool_table[index].backangle, toolResult.backangle):
                self.status.io.tool_table[index].backangle = toolResult.backangle
                txToolResult.backangle = toolResult.backangle
                toolResultModified = True

            if self.status.io.tool_table[index].orientation != toolResult.orientation:
                self.status.io.tool_table[index].orientation = toolResult.orientation
                txToolResult.orientation = toolResult.orientation
                toolResultModified = True

            if toolResultModified:
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
            self.taskFirstrun = False

        if (self.status.task.echo_serial_number != stat.echo_serial_number):
            self.status.task.echo_serial_number = stat.echo_serial_number
            self.statusTx.task.echo_serial_number = stat.echo_serial_number
            modified = True

        if (self.status.task.exec_state != stat.exec_state):
            self.status.task.exec_state = stat.exec_state
            self.statusTx.task.exec_state = stat.exec_state
            modified = True

        if (self.status.task.file != stat.file):
            self.status.task.file = stat.file
            self.statusTx.task.file = stat.file
            modified = True

        if (self.status.task.input_timeout != stat.input_timeout):
            self.status.task.input_timeout = stat.input_timeout
            self.statusTx.task.input_timeout = stat.input_timeout
            modified = True

        if (self.status.task.optional_stop != stat.optional_stop):
            self.status.task.optional_stop = stat.optional_stop
            self.statusTx.task.optional_stop = stat.optional_stop
            modified = True

        if (self.status.task.read_line != stat.read_line):
            self.status.task.read_line = stat.read_line
            self.statusTx.task.read_line = stat.read_line
            modified = True

        if (self.status.task.task_mode != stat.task_mode):
            self.status.task.task_mode = stat.task_mode
            self.statusTx.task.task_mode = stat.task_mode
            modified = True

        if (self.status.task.task_paused != stat.task_paused):
            self.status.task.task_paused = stat.task_paused
            self.statusTx.task.task_paused = stat.task_paused
            modified = True

        if (self.status.task.task_state != stat.task_state):
            self.status.task.task_state = stat.task_state
            self.statusTx.task.task_state = stat.task_state
            modified = True

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

        if (self.status.interp.command != stat.command):
            self.status.interp.command = stat.command
            self.statusTx.interp.command = stat.command
            modified = True

        txStatusGCode = EmcStatusGCode()
        for index, gcode in enumerate(stat.gcodes):
            txStatusGCode.Clear()
            gcodeModified = False

            if len(self.status.interp.gcodes) == index:
                self.status.interp.gcodes.add()
                self.status.interp.gcodes[index].index = index
                self.status.interp.gcodes[index].value = 0

            if self.status.interp.gcodes[index].value != gcode:
                self.status.interp.gcodes[index].value = gcode
                txStatusGCode.value = gcode
                gcodeModified = True

            if gcodeModified:
                txStatusGCode.index = index
                self.statusTx.interp.gcodes.add().CopyFrom(txStatusGCode)
                modified = True
        del txStatusGCode

        if (self.status.interp.interp_state != stat.interp_state):
            self.status.interp.interp_state = stat.interp_state
            self.statusTx.interp.interp_state = stat.interp_state
            modified = True

        if (self.status.interp.interpreter_errcode != stat.interpreter_errcode):
            self.status.interp.interpreter_errcode = stat.interpreter_errcode
            self.statusTx.interp.interpreter_errcode = stat.interpreter_errcode
            modified = True

        txStatusMCode = EmcStatusMCode()
        for index, mcode in enumerate(stat.mcodes):
            txStatusMCode.Clear()
            mcodeModified = False

            if len(self.status.interp.mcodes) == index:
                self.status.interp.mcodes.add()
                self.status.interp.mcodes[index].index = index
                self.status.interp.mcodes[index].value = 0

            if self.status.interp.mcodes[index].value != mcode:
                self.status.interp.mcodes[index].value = mcode
                txStatusMCode.value = mcode
                mcodeModified = True

            if mcodeModified:
                txStatusMCode.index = index
                self.statusTx.interp.mcodes.add().CopyFrom(txStatusMCode)
                modified = True
        del txStatusMCode

        txStatusSetting = EmcStatusSetting()
        for index, setting in enumerate(stat.settings):
            txStatusSetting.Clear()
            settingModified = False

            if len(self.status.interp.settings) == index:
                self.status.interp.settings.add()
                self.status.interp.settings[index].index = index
                self.status.interp.settings[index].value = 0.0

            if self.notEqual(self.status.interp.settings[index].value, setting):
                self.status.interp.settings[index].value = setting
                txStatusSetting.value = setting
                settingModified = True

            if settingModified:
                txStatusSetting.index = index
                self.statusTx.interp.settings.add().CopyFrom(txStatusSetting)
                modified = True
        del txStatusSetting

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

        if (self.status.motion.active_queue != stat.active_queue):
            self.status.motion.active_queue = stat.active_queue
            self.statusTx.motion.active_queue = stat.active_queue
            modified = True

        positionModified = False
        txPosition = None
        positionModified, txPosition = self.check_position(self.status.motion.actual_position, stat.actual_position)
        if positionModified:
            self.status.motion.actual_position.MergeFrom(txPosition)
            self.statusTx.motion.actual_position.CopyFrom(txPosition)
            modified = True
        del txPosition

        if (self.status.motion.adaptive_feed_enabled != stat.adaptive_feed_enabled):
            self.status.motion.adaptive_feed_enabled = stat.adaptive_feed_enabled
            self.statusTx.motion.adaptive_feed_enabled = stat.adaptive_feed_enabled
            modified = True

        txAin = EmcStatusAnalogIO()
        for index, ain in enumerate(stat.ain):
            txAin.Clear()
            ainModified = False

            if len(self.status.motion.ain) == index:
                self.status.motion.ain.add()
                self.status.motion.ain[index].index = index
                self.status.motion.ain[index].value = 0.0

            if self.notEqual(self.status.motion.ain[index].value, ain):
                self.status.motion.ain[index].value = ain
                txAin.value = ain
                ainModified = True

            if ainModified:
                txAin.index = index
                self.statusTx.motion.ain.add().CopyFrom(txAin)
                modified = True
        del txAin

        txAout = EmcStatusAnalogIO()
        for index, aout in enumerate(stat.aout):
            txAout.Clear()
            aoutModified = False

            if len(self.status.motion.aout) == index:
                self.status.motion.aout.add()
                self.status.motion.aout[index].index = index
                self.status.motion.aout[index].value = 0.0

            if self.notEqual(self.status.motion.aout[index].value, aout):
                self.status.motion.aout[index].value = aout
                txAout.value = aout
                aoutModified = True

            if aoutModified:
                txAout.index = index
                self.statusTx.motion.aout.add().CopyFrom(txAout)
                modified = True
        del txAout

        txAxis = EmcStatusMotionAxis()
        for index, axis in enumerate(stat.axis):
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

            if self.status.motion.axis[index].enabled != axis['enabled']:
                self.status.motion.axis[index].enabled = axis['enabled']
                txAxis.enabled = axis['enabled']
                axisModified = True

            if self.status.motion.axis[index].fault != axis['fault']:
                self.status.motion.axis[index].fault = axis['fault']
                txAxis.fault = axis['fault']
                axisModified = True

            if self.notEqual(self.status.motion.axis[index].ferror_current, axis['ferror_current']):
                self.status.motion.axis[index].ferror_current = axis['ferror_current']
                txAxis.ferror_current = axis['ferror_current']
                axisModified = True

            if self.notEqual(self.status.motion.axis[index].ferror_highmark, axis['ferror_highmark']):
                self.status.motion.axis[index].ferror_highmark = axis['ferror_highmark']
                txAxis.ferror_highmark = axis['ferror_highmark']
                axisModified = True

            if self.status.motion.axis[index].homed != axis['homed']:
                self.status.motion.axis[index].homed = axis['homed']
                txAxis.homed = axis['homed']
                axisModified = True

            if self.status.motion.axis[index].homing != axis['homing']:
                self.status.motion.axis[index].homing = axis['homing']
                txAxis.homing = axis['homing']
                axisModified = True

            if self.status.motion.axis[index].inpos != axis['inpos']:
                self.status.motion.axis[index].inpos = axis['inpos']
                txAxis.inpos = axis['inpos']
                axisModified = True

            if self.notEqual(self.status.motion.axis[index].input, axis['input']):
                self.status.motion.axis[index].input = axis['input']
                txAxis.input = axis['input']
                axisModified = True

            if self.status.motion.axis[index].max_hard_limit != axis['max_hard_limit']:
                self.status.motion.axis[index].max_hard_limit = axis['max_hard_limit']
                txAxis.max_hard_limit = axis['max_hard_limit']
                axisModified = True

            if self.status.motion.axis[index].max_soft_limit != axis['max_soft_limit']:
                self.status.motion.axis[index].max_soft_limit = axis['max_soft_limit']
                txAxis.max_soft_limit = axis['max_soft_limit']
                axisModified = True

            if self.status.motion.axis[index].min_hard_limit != axis['min_hard_limit']:
                self.status.motion.axis[index].min_hard_limit = axis['min_hard_limit']
                txAxis.min_hard_limit = axis['min_hard_limit']
                axisModified = True

            if self.status.motion.axis[index].min_soft_limit != axis['min_soft_limit']:
                self.status.motion.axis[index].min_soft_limit = axis['min_soft_limit']
                txAxis.min_soft_limit = axis['min_soft_limit']
                axisModified = True

            if self.notEqual(self.status.motion.axis[index].output, axis['output']):
                self.status.motion.axis[index].output = axis['output']
                txAxis.output = axis['output']
                axisModified = True

            if self.status.motion.axis[index].override_limits != axis['override_limits']:
                self.status.motion.axis[index].override_limits = axis['override_limits']
                txAxis.override_limits = axis['override_limits']
                axisModified = True

            if self.notEqual(self.status.motion.axis[index].velocity, axis['velocity']):
                self.status.motion.axis[index].velocity = axis['velocity']
                txAxis.velocity = axis['velocity']
                axisModified = True

            if axisModified:
                txAxis.index = index
                self.statusTx.motion.axis.add().CopyFrom(txAxis)
                modified = True
        del txAxis

        if (self.status.motion.block_delete != stat.block_delete):
            self.status.motion.block_delete = stat.block_delete
            self.statusTx.motion.block_delete = stat.block_delete
            modified = True

        if (self.status.motion.current_line != stat.current_line):
            self.status.motion.current_line = stat.current_line
            self.statusTx.motion.current_line = stat.current_line
            modified = True

        if self.notEqual(self.status.motion.current_vel, stat.current_vel):
            self.status.motion.current_vel = stat.current_vel
            self.statusTx.motion.current_vel = stat.current_vel
            modified = True

        if self.notEqual(self.status.motion.delay_left, stat.delay_left):
            self.status.motion.delay_left = stat.delay_left
            self.statusTx.motion.delay_left = stat.delay_left
            modified = True

        txDin = EmcStatusDigitalIO()
        for index, din in enumerate(stat.din):
            txDin.Clear()
            dinModified = False

            if len(self.status.motion.din) == index:
                self.status.motion.din.add()
                self.status.motion.din[index].index = index
                self.status.motion.din[index].value = False

            if self.status.motion.din[index].value != din:
                self.status.motion.din[index].value = din
                txDin.value = din
                dinModified = True

            if dinModified:
                txDin.index = index
                self.statusTx.motion.din.add().CopyFrom(txDin)
                modified = True
        del txDin

        if self.notEqual(self.status.motion.distance_to_go, stat.distance_to_go):
            self.status.motion.distance_to_go = stat.distance_to_go
            self.statusTx.motion.distance_to_go = stat.distance_to_go
            modified = True

        txDout = EmcStatusDigitalIO()
        for index, dout in enumerate(stat.dout):
            txDout.Clear()
            doutModified = False

            if len(self.status.motion.dout) == index:
                self.status.motion.dout.add()
                self.status.motion.dout[index].index = index
                self.status.motion.dout[index].value = False

            if self.status.motion.dout[index].value != dout:
                self.status.motion.dout[index].value = dout
                txDout.value = dout
                doutModified = True

            if doutModified:
                txDout.index = index
                self.statusTx.motion.dout.add().CopyFrom(txDout)
                modified = True
        del txDout

        positionModified, txPosition = self.check_position(self.status.motion.dtg, stat.dtg)
        if positionModified:
            self.status.motion.dtg.MergeFrom(txPosition)
            self.statusTx.motion.dtg.CopyFrom(txPosition)
            modified = True
        del txPosition

        if (self.status.motion.enabled != stat.enabled):
            self.status.motion.enabled = stat.enabled
            self.statusTx.motion.enabled = stat.enabled
            modified = True

        if (self.status.motion.feed_hold_enabled != stat.feed_hold_enabled):
            self.status.motion.feed_hold_enabled = stat.feed_hold_enabled
            self.statusTx.motion.feed_hold_enabled = stat.feed_hold_enabled
            modified = True

        if (self.status.motion.feed_override_enabled != stat.feed_override_enabled):
            self.status.motion.feed_override_enabled = stat.feed_override_enabled
            self.statusTx.motion.feed_override_enabled = stat.feed_override_enabled
            modified = True

        if self.notEqual(self.status.motion.feedrate, stat.feedrate):
            self.status.motion.feedrate = stat.feedrate
            self.statusTx.motion.feedrate = stat.feedrate
            modified = True

        if (self.status.motion.g5x_index != stat.g5x_index):
            self.status.motion.g5x_index = stat.g5x_index
            self.statusTx.motion.g5x_index = stat.g5x_index
            modified = True

        positionModified, txPosition = self.check_position(self.status.motion.g5x_offset, stat.g5x_offset)
        if positionModified:
            self.status.motion.g5x_offset.MergeFrom(txPosition)
            self.statusTx.motion.g5x_offset.CopyFrom(txPosition)
            modified = True
        del txPosition

        positionModified, txPosition = self.check_position(self.status.motion.g92_offset, stat.g92_offset)
        if positionModified:
            self.status.motion.g92_offset.MergeFrom(txPosition)
            self.statusTx.motion.g92_offset.CopyFrom(txPosition)
            modified = True
        del txPosition

        if (self.status.motion.id != stat.id):
            self.status.motion.id = stat.id
            self.statusTx.motion.id = stat.id
            modified = True

        if (self.status.motion.inpos != stat.inpos):
            self.status.motion.inpos = stat.inpos
            self.statusTx.motion.inpos = stat.inpos
            modified = True

        positionModified, txPosition = self.check_position(self.status.motion.joint_actual_position, stat.joint_actual_position)
        if positionModified:
            self.status.motion.joint_actual_position.MergeFrom(txPosition)
            self.statusTx.motion.joint_actual_position.CopyFrom(txPosition)
            modified = True
        del txPosition

        positionModified, txPosition = self.check_position(self.status.motion.joint_position, stat.joint_position)
        if positionModified:
            self.status.motion.joint_position.MergeFrom(txPosition)
            self.statusTx.motion.joint_position.CopyFrom(txPosition)
            modified = True
        del txPosition

        txLimit = EmcStatusLimit()
        for index, limit in enumerate(stat.limit):
            txLimit.Clear()
            limitModified = False

            if index == stat.axes:
                break

            if len(self.status.motion.limit) == index:
                self.status.motion.limit.add()
                self.status.motion.limit[index].index = index
                self.status.motion.limit[index].value = False

            if self.status.motion.limit[index].value != limit:
                self.status.motion.limit[index].value = limit
                txLimit.value = limit
                limitModified = True

            if limitModified:
                txLimit.index = index
                self.statusTx.motion.limit.add().CopyFrom(txLimit)
                modified = True
        del txLimit

        if (self.status.motion.motion_line != stat.motion_line):
            self.status.motion.motion_line = stat.motion_line
            self.statusTx.motion.motion_line = stat.motion_line
            modified = True

        if (self.status.motion.motion_type != stat.motion_type):
            self.status.motion.motion_type = stat.motion_type
            self.statusTx.motion.motion_type = stat.motion_type
            modified = True

        if (self.status.motion.motion_mode != stat.motion_mode):
            self.status.motion.motion_mode = stat.motion_mode
            self.statusTx.motion.motion_mode = stat.motion_mode
            modified = True

        if (self.status.motion.paused != stat.paused):
            self.status.motion.paused = stat.paused
            self.statusTx.motion.paused = stat.paused
            modified = True

        positionModified, txPosition = self.check_position(self.status.motion.position, stat.position)
        if positionModified:
            self.status.motion.position.MergeFrom(txPosition)
            self.statusTx.motion.position.CopyFrom(txPosition)
            modified = True
        del txPosition

        if (self.status.motion.probe_tripped != stat.probe_tripped):
            self.status.motion.probe_tripped = stat.probe_tripped
            self.statusTx.motion.probe_tripped = stat.probe_tripped
            modified = True

        if (self.status.motion.probe_val != stat.probe_val):
            self.status.motion.probe_val = stat.probe_val
            self.statusTx.motion.probe_val = stat.probe_val
            modified = True

        positionModified, txPosition = self.check_position(self.status.motion.probed_position, stat.probed_position)
        if positionModified:
            self.status.motion.probed_position.MergeFrom(txPosition)
            self.statusTx.motion.probed_position.CopyFrom(txPosition)
            modified = True
        del txPosition

        if (self.status.motion.probing != stat.probing):
            self.status.motion.probing = stat.probing
            self.statusTx.motion.probing = stat.probing
            modified = True

        if (self.status.motion.queue != stat.queue):
            self.status.motion.queue = stat.queue
            self.statusTx.motion.queue = stat.queue
            modified = True

        if (self.status.motion.queue_full != stat.queue_full):
            self.status.motion.queue_full = stat.queue_full
            self.statusTx.motion.queue_full = stat.queue_full
            modified = True

        if self.notEqual(self.status.motion.rotation_xy, stat.rotation_xy):
            self.status.motion.rotation_xy = stat.rotation_xy
            self.statusTx.motion.rotation_xy = stat.rotation_xy
            modified = True

        if (self.status.motion.spindle_brake != stat.spindle_brake):
            self.status.motion.spindle_brake = stat.spindle_brake
            self.statusTx.motion.spindle_brake = stat.spindle_brake
            modified = True

        if (self.status.motion.spindle_direction != stat.spindle_direction):
            self.status.motion.spindle_direction = stat.spindle_direction
            self.statusTx.motion.spindle_direction = stat.spindle_direction
            modified = True

        if (self.status.motion.spindle_enabled != stat.spindle_enabled):
            self.status.motion.spindle_enabled = stat.spindle_enabled
            self.statusTx.motion.spindle_enabled = stat.spindle_enabled
            modified = True

        if (self.status.motion.spindle_increasing != stat.spindle_increasing):
            self.status.motion.spindle_increasing = stat.spindle_increasing
            self.statusTx.motion.spindle_increasing = stat.spindle_increasing
            modified = True

        if (self.status.motion.spindle_override_enabled != stat.spindle_override_enabled):
            self.status.motion.spindle_override_enabled = stat.spindle_override_enabled
            self.statusTx.motion.spindle_override_enabled = stat.spindle_override_enabled
            modified = True

        if self.notEqual(self.status.motion.spindle_speed, stat.spindle_speed):
            self.status.motion.spindle_speed = stat.spindle_speed
            self.statusTx.motion.spindle_speed = stat.spindle_speed
            modified = True

        if self.notEqual(self.status.motion.spindlerate, stat.spindlerate):
            self.status.motion.spindlerate = stat.spindlerate
            self.statusTx.motion.spindlerate = stat.spindlerate
            modified = True

        if (self.status.motion.state != stat.state):
            self.status.motion.state = stat.state
            self.statusTx.motion.state = stat.state
            modified = True

        if self.notEqual(self.status.motion.max_acceleration, stat.max_acceleration):
            self.status.motion.max_acceleration = stat.max_acceleration
            self.statusTx.motion.max_acceleration = stat.max_acceleration
            modified = True

        if self.notEqual(self.status.motion.max_velocity, stat.max_velocity):
            self.status.motion.max_velocity = stat.max_velocity
            self.statusTx.motion.max_velocity = stat.max_velocity
            modified = True

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
        if len(self.linuxcncErrors) > 0:
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
        self.txStatus.type = type
        txBuffer = self.txStatus.SerializeToString()
        self.txStatus.Clear()
        self.statusSocket.send_multipart([topic, txBuffer])

    def send_error_msg(self, topic, type):
        self.txError.type = type
        txBuffer = self.txError.SerializeToString()
        self.txError.Clear()
        self.errorSocket.send_multipart([topic, txBuffer])

    def send_command_msg(self, type):
        self.txCommand.type = type
        txBuffer = self.txCommand.SerializeToString()
        self.txCommand.Clear()
        self.commandSocket.send(txBuffer)

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
            rc = socket.recv(zmq.NOBLOCK)
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
            rc = socket.recv(zmq.NOBLOCK)
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

    def send_command_wrong_params(self):
        self.txCommand.note.append("wrong parameters")
        self.send_command_msg(MT_ERROR)

    def process_command(self, socket):
        if self.debug:
            print("process command called")

        message = socket.recv()
        self.rx.ParseFromString(message)

        try:
            if self.rx.type == MT_PING:
                self.send_command_msg(MT_PING_ACKNOWLEDGE)

            elif self.rx.type == MT_EMC_TASK_ABORT:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.abort()
                    elif self.rx.interp_name == 'preview':
                        self.preview.abort()
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_PAUSE:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.auto(linuxcnc.AUTO_PAUSE)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_RESUME:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.auto(linuxcnc.AUTO_RESUME)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_STEP:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.auto(linuxcnc.AUTO_STEP)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_RUN:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('line_number') \
                and self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        lineNumber = self.rx.emc_command_params.line_number
                        self.command.auto(linuxcnc.AUTO_RUN, lineNumber)
                    elif self.rx.interp_name == 'preview':
                        self.preview.start()
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_SPINDLE_BRAKE_ENGAGE:
                self.command.brake(linuxcnc.BRAKE_ENGAGE)

            elif self.rx.type == MT_EMC_SPINDLE_BRAKE_RELEASE:
                self.command.brake(linuxcnc.BRAKE_RELEASE)

            elif self.rx.type == MT_EMC_SET_DEBUG:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('debug_level'):
                    debugLevel = self.rx.emc_command_params.debug_level
                    self.command.debug(debugLevel)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TRAJ_SET_SCALE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('scale'):
                    feedrate = self.rx.emc_command_params.scale
                    self.command.feedrate(feedrate)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_COOLANT_FLOOD_ON:
                self.command.flood(linuxcnc.FLOOD_ON)

            elif self.rx.type == MT_EMC_COOLANT_FLOOD_OFF:
                self.command.flood(linuxcnc.FLOOD_OFF)

            elif self.rx.type == MT_EMC_AXIS_HOME:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.home(axis)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_ABORT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.jog(linuxcnc.JOG_STOP, axis)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_JOG:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('velocity'):
                    axis = self.rx.emc_command_params.index
                    velocity = self.rx.emc_command_params.velocity
                    self.command.jog(linuxcnc.JOG_CONTINUOUS, axis, velocity)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_INCR_JOG:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('velocity') \
                and self.rx.emc_command_params.HasField('distance'):
                    axis = self.rx.emc_command_params.index
                    velocity = self.rx.emc_command_params.velocity
                    distance = self.rx.emc_command_params.distance
                    self.command.jog(linuxcnc.JOG_INCREMENT, axis, velocity, distance)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TOOL_LOAD_TOOL_TABLE:
                self.command.load_tool_table()

            elif self.rx.type == MT_EMC_TRAJ_SET_MAX_VELOCITY:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('velocity'):
                    velocity = self.rx.emc_command_params.velocity
                    self.command.maxvel(velocity)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_EXECUTE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('command') \
                and self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        command = self.rx.emc_command_params.command
                        self.command.mdi(command)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_COOLANT_MIST_ON:
                self.command.mist(linuxcnc.MIST_ON)

            elif self.rx.type == MT_EMC_COOLANT_MIST_OFF:
                self.command.mist(linuxcnc.MIST_OFF)

            elif self.rx.type == MT_EMC_TASK_SET_MODE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('task_mode') \
                and self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.mode(self.rx.emc_command_params.task_mode)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_OVERRIDE_LIMITS:
                self.command.override_limits()

            elif self.rx.type == MT_EMC_TASK_PLAN_OPEN:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('path') \
                and self.rx.HasField('interp_name'):
                    fileName = self.rx.emc_command_params.path
                    fileName = self.preprocess_program(fileName)
                    if fileName is not '':
                        if self.rx.interp_name == 'execute':
                            self.command.program_open(fileName)
                        elif self.rx.interp_name == 'preview':
                            self.preview.program_open(fileName)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_INIT:
                if self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.reset_interpreter()
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_MOTION_ADAPTIVE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    adaptiveFeed = self.rx.emc_command_params.enable
                    self.command.set_adaptive_feed(adaptiveFeed)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_MOTION_SET_AOUT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('value'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_analog_output(axis, value)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_SET_BLOCK_DELETE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    blockDelete = self.rx.emc_command_params.enable
                    self.command.set_block_delete(blockDelete)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_MOTION_SET_DOUT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('enable'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.enable
                    self.command.set_digital_output(axis, value)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TRAJ_SET_FH_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    feedHold = self.rx.emc_command_params.enable
                    self.command.set_feed_hold(feedHold)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TRAJ_SET_FO_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    feedOverride = self.rx.emc_command_params.enable
                    self.command.set_feed_override(feedOverride)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_SET_MAX_POSITION_LIMIT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('value'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_max_limit(axis, value)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_SET_MIN_POSITION_LIMIT:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index') \
                and self.rx.emc_command_params.HasField('value'):
                    axis = self.rx.emc_command_params.index
                    value = self.rx.emc_command_params.value
                    self.command.set_min_limit(axis, value)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_PLAN_SET_OPTIONAL_STOP:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    optionalStop = self.rx.emc_command_params.enable
                    self.command.set_optional_stop(optionalStop)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TRAJ_SET_SO_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    spindleOverride = self.rx.emc_command_params.enable
                    self.command.set_spindle_override(spindleOverride)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_SPINDLE_ON:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('velocity'):
                    speed = self.rx.emc_command_params.velocity
                    direction = linuxcnc.SPINDLE_FORWARD    # always forwward, speed can be signed
                    self.command.spindle(direction, speed)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_SPINDLE_INCREASE:
                self.command.spindle(linuxcnc.SPINDLE_INCREASE)

            elif self.rx.type == MT_EMC_SPINDLE_DECREASE:
                self.command.spindle(linuxcnc.SPINDLE_DECREASE)

            elif self.rx.type == MT_EMC_SPINDLE_CONSTANT:
                self.command.spindle(linuxcnc.SPINDLE_CONSTANT)

            elif self.rx.type == MT_EMC_SPINDLE_OFF:
                self.command.spindle(linuxcnc.SPINDLE_OFF)

            elif self.rx.type == MT_EMC_TRAJ_SET_SPINDLE_SCALE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('scale'):
                    scale = self.rx.emc_command_params.scale
                    self.command.spindleoverride(scale)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TASK_SET_STATE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('task_state') \
                and self.rx.HasField('interp_name'):
                    if self.rx.interp_name == 'execute':
                        self.command.state(self.rx.emc_command_params.task_state)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TRAJ_SET_TELEOP_ENABLE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('enable'):
                    teleopEnable = self.rx.emc_command_params.enable
                    self.command.teleop_enable(teleopEnable)
                else:
                    self.send_command_wrong_params()

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
                else:
                    self.send_command_wrong_params()

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
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_TRAJ_SET_MODE:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('traj_mode'):
                    self.command.traj_mode(self.rx.emc_command_params.traj_mode)
                else:
                    self.send_command_wrong_params()

            elif self.rx.type == MT_EMC_AXIS_UNHOME:
                if self.rx.HasField('emc_command_params') \
                and self.rx.emc_command_params.HasField('index'):
                    axis = self.rx.emc_command_params.index
                    self.command.unhome(axis)
                else:
                    self.send_command_wrong_params()

            else:
                self.txCommand.note.append("unknown command")
                self.send_command_msg(MT_ERROR)

        except linuxcnc.error as detail:
            self.linuxcncErrors.append(detail)
        except UnicodeEncodeError:
            self.linuxcncErrors.append("Please use only ASCII characters")
        except Exception as e:
            printError('uncaught exception ' + str(e))
            self.linuxcncErrors.append(str(e))


shutdown = False


def _exitHandler(signum, frame):
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
        hostname = socket.gethostname().split('.')[0] + '.local.'
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
    while threading.active_count() > 1:
        time.sleep(0.1)

    print("threads stopped")
    sys.exit(0)

if __name__ == "__main__":
    main()
