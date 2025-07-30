#!/usr/bin/env python3

import os
import sys
import time

import json
import signal

import hal
from PyQt5 import QtCore
from qtvcp.qt_halobjects import Qhal
from common.iniinfo import _IStat as IStatParent
from common import logger

# LOG is for running code logging
LOG = logger.initBaseLogger('HAL bridge', log_file=None,
             log_level=logger.WARNING, logToFile=False)

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

try:
    import zmq
    ZMQ = True
except:
    LOG.critical('ZMQ python library problem  - Is python3-zmq installed?')
    ZMQ = False

class Info(IStatParent):
    _instance = None
    _instanceNum = 0

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = IStatParent.__new__(cls, *args, **kwargs)
        return cls._instance

# Instantiate the library with global reference


class Bridge(object):
    def __init__(self, readAddress = "tcp://127.0.0.1:5690",
                     writeAddress = "tcp://127.0.0.1:5691"):
        super(Bridge, self).__init__()
        self.INFO = Info()

        self.readAddress = readAddress
        self.writeAddress = writeAddress
        LOG.debug('read port: {}'.format(readAddress))
        LOG.debug('write port: {}'.format(writeAddress))

        self.readTopic = ""
        self.writeTopic = "STATUSREQUEST"

        # catch control c and terminate signals
        signal.signal(signal.SIGTERM, self.shutdown)
        signal.signal(signal.SIGINT, self.shutdown)

        self.init_hal()
        if ZMQ:
            self.init_read()
            self.init_write()

    def init_hal(self):
        self.comp = h = hal.component("bridge")
        QHAL = Qhal(comp=self.comp, hal=hal)

        self.jogRate = QHAL.newpin("jog-rate", hal.HAL_FLOAT, hal.HAL_OUT)
        self.jogRateIn = QHAL.newpin("jog-rate-in", hal.HAL_FLOAT, hal.HAL_IN)
        self.jogRateIn.pinValueChanged.connect(self.pinChanged)

        self.jogRateAngular = QHAL.newpin("jog-rate-angular", hal.HAL_FLOAT, hal.HAL_OUT)
        self.jogRateAngularIn = QHAL.newpin("jog-rate-angular-in", hal.HAL_FLOAT, hal.HAL_OUT)
        self.jogRateAngularIn.pinValueChanged.connect(self.pinChanged)

        self.jogIncrement = QHAL.newpin("jog-increment", hal.HAL_FLOAT, hal.HAL_OUT)
        self.jogIncrementAngular = QHAL.newpin("jog-increment-angular", hal.HAL_FLOAT, hal.HAL_OUT)
        self.activeJoint = QHAL.newpin('joint-selected', hal.HAL_S32, hal.HAL_OUT)

        for i in (self.INFO.AVAILABLE_AXES):
            let = i.lower()
            # input
            self['axis{}Select'.format(let)] = QHAL.newpin('axis-%s-select'%let, hal.HAL_BIT, hal.HAL_IN)
            self['axis{}Select'.format(let)].pinValueChanged.connect(self.pinChanged)
            # output
            self['Axis{}IsSelected'.format(let)] = QHAL.newpin('axis-%s-is-selected'%let, hal.HAL_BIT, hal.HAL_OUT)

        self.cycle_start = QHAL.newpin('cycle-start-in',QHAL.HAL_BIT, QHAL.HAL_IN)
        self.cycle_start.pinValueChanged.connect(self.pinChanged)
        self.cycle_pause = QHAL.newpin('cycle-pause-in',QHAL.HAL_BIT, QHAL.HAL_IN)
        self.cycle_pause.pinValueChanged.connect(self.pinChanged)

        for i in self.INFO.MDI_COMMAND_DICT:
            LOG.debug('{} {}'.format(i,self.INFO.MDI_COMMAND_DICT.get(i)))
            self[i] = QHAL.newpin('macro-cmd-{}'.format(i),QHAL.HAL_BIT, QHAL.HAL_IN)
            self[i].pinValueChanged.connect(self.runMacroChanged)

        QHAL.setUpdateRate(100)
        h.ready()

    def init_write(self):
        context = zmq.Context()
        self.writeSocket = context.socket(zmq.PUB)
        self.writeSocket.bind(self.writeAddress)

    def init_read(self):
        # ZeroMQ Context
        self.readContext = zmq.Context()

        # Define the socket using the "Context"
        self.readSocket = self.readContext.socket(zmq.SUB)

        # Define subscription and messages with topic to accept.
        self.readSocket.setsockopt_string(zmq.SUBSCRIBE, self.readTopic)
        self.readSocket.connect(self.readAddress)
        self.readNotify = QtCore.QSocketNotifier(
                                    self.readSocket.getsockopt(zmq.FD),
                                    QtCore.QSocketNotifier.Read, None)
        self.readNotify.activated.connect(self.onReadMsg)

    # callback from ZMQ read socket
    def onReadMsg(self, msg):
        if self.readSocket.getsockopt(zmq.EVENTS) & zmq.POLLIN:
            while self.readSocket.getsockopt(zmq.EVENTS) & zmq.POLLIN:
                # get raw message
                topic, data = self.readSocket.recv_multipart()
                # convert from json object to python object
                y = json.loads(data)
                self. action(y.get('MESSAGE'),y.get('ARGS'))

    # set our output HAL pins from messages from hal_glib
    def action(self, msg, data):
        LOG.debug('{} {}'.format(msg, data))
        if msg == 'jograte-changed':
            self.jogRate.set(float(data[0]))
        if msg == 'jograte-angular-changed':
            self.jogRateAngular.set(float(data[0]))
        elif msg == 'jogincrements-changed':
            self.jogIncrement.set(float(data[0][0]))
        elif msg == 'jogincrement-angular-changed':
            self.jogIncremtAngular.set(float(data[0][0]))
        elif msg == 'joint-selection-changed':
            self.activeJoint.set(int(data[0]))
        elif msg == 'axis-selection-changed':
            for i in(self.INFO.AVAILABLE_AXES):
                if data[0] == i:
                    state = True
                else:
                    state = False
                self['Axis{}IsSelected'.format(i.lower())].set(state)

    # send msg to hal_glib
    def writeMsg(self, msg, data):
        if ZMQ:
            topic = self.writeTopic
            message = json.dumps({'FUNCTION':msg,'ARGS':data})
            LOG.debug('Sending ZMQ Message:{} {}'.format(topic, message))
            self.writeSocket.send_multipart(
                        [bytes(topic.encode('utf-8')),
                            bytes((message).encode('utf-8'))])

    # callback from HAL input pins
    def pinChanged(self, pinObject, value):
        LOG.debug('Pin name:{} changed value to {}'.format(pinObject.text(), value))
        #print(type(value))
        # Axis selction change request
        if 'select' in pinObject.text():
            if bool(value) == False:
                pass
                #print('Not true state')
                return
            for i in (self.INFO.AVAILABLE_AXES):
                if '-{}-'.format(i.lower()) in pinObject.text():
                    self.writeMsg('set_selected_axis', i)
                    break
            else:
                if 'None' in pinObject.text():
                    self.writeMsg('set_selected_axis', '')

        # cycle start
        elif self.cycle_start == pinObject:
            if value:
                self.writeMsg('request_cycle_start', value)

        # cycle pause
        elif self.cycle_pause == pinObject:
            #if value:
                self.writeMsg('request_cycle_pause', value)

        # linear jog rate
        elif self.jogRateIn == pinObject:
                self.writeMsg('set_jograte', value)

       # angular jog rate
        elif self.jogRateAngularIn == pinObject:
                self.writeMsg('set_jograte_angular', value)

        # catch all default
        else:
            self.writeMsg(pinObject.text(),value)

    # callback; request to run a specific macro
    def runMacroChanged(self, pinObject, value):
        LOG.debug('Macro Pin name:{} changed value to {}'.format(pinObject.text(), value))
        #LOG.debug(type(value))
        name = pinObject.text().strip('macro-cmd-')
        if value:
            self.writeMsg('request_macro_call', name)

    def shutdown(self,signum=None,stack_frame=None):
        LOG.debug('shutdown')
        global app
        app.quit()

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    import sys
    import getopt
    from PyQt5.QtWidgets import QApplication

    letters = 'dh' # the : means an argument needs to be passed after the letter
    keywords = ['readport=', 'writeport=' ] # the = means that a value is expected after 
    # the keyword

    opts, extraparam = getopt.getopt(sys.argv[1:],letters,keywords) 
    # starts at the second element of argv since the first one is the script name
    # extraparms are extra arguments passed after all option/keywords are assigned
    # opts is a list containing the pair "option"/"value"

    readport = "tcp://127.0.0.1:5690"
    writeport = "tcp://127.0.0.1:5691"

    for o,p in opts:
        if o in ['-d']:
            LOG.setLevel(logger.DEBUG)
        elif o in ['--readport']:
            readport = p
        elif o in ['--writeport']:
            writeport = p
        elif o in ['-h','--help']:
            print('HAL bridge: GUI to HAL interface using ZMQ')
            print('option "-d" = debug print mode')
            print('option "--readport=" read socket address')
            print('option "--writeport=" write socket address')
            print('example: hal_bridge -d --readport=tcp://127.0.0.1:5692')

    app = QApplication(sys.argv)
    test = Bridge(readport, writeport)
    sys.exit(app.exec_())
