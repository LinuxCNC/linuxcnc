#!/usr/bin/env python3

import os
import sys
import time

import json
import signal

import hal
from common.iniinfo import _IStat as IStatParent
from common import logger

# LOG is for running code logging
LOG = logger.initBaseLogger('HAL bridge', log_file=None,
             log_level=logger.WARNING, logToFile=False)

# Force the log level for this module
LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

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

        self.currentSelectedAxis = 'None'
        self.axesSelected = {'X':0,'Y':0,'Z':0,'A':0,'B':0,'C':0,
                                'U':0,'V':0,'W':0,'MPG0':0}
        self.readAddress = readAddress
        self.writeAddress = writeAddress
        LOG.debug('read port: {}'.format(readAddress))
        LOG.debug('write port: {}'.format(writeAddress))

        self.readTopic = ""
        self.writeTopic = "STATUSREQUEST"

        # catch control c and terminate signals
        signal.signal(signal.SIGTERM, self.shutdown)
        signal.signal(signal.SIGINT, self.shutdown)

        self.init()
        if ZMQ:
            self.init_read()
            self.init_write()

    def update(self, *arg):
        print(self, arg)
        raw=arg[0]; row=arg[1];column=arg[2];state=arg[3]
        LOG.debug('raw {}, row {}, col {}, state {}'.format(raw,row,column,state))
        print ('raw',raw,'row:',row,'column:',column,'state:',state)
        self.writeMsg('set_selected_axis','Y')
        self.activeJoint.set(10)

    def init(self):
        self.jogRate = 0
        self.jogRateAngular = 0

        self.jogIncrement = 0
        self.jogIncrementAngular = 0

        self.activeJoint = 0

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

    # callback from ZMQ read socket
    def readMsg(self):
        if self.readSocket.getsockopt(zmq.EVENTS) & zmq.POLLIN:
            while self.readSocket.getsockopt(zmq.EVENTS) & zmq.POLLIN:
                # get raw message
                topic, data = self.readSocket.recv_multipart()
                # convert from json object to python object
                y = json.loads(data)
                self. action(y.get('MESSAGE'),y.get('ARGS'))

    # set our variables from messages from hal_glib
    def action(self, msg, data):
        LOG.debug('{} -> {} -> {}'.format(msg, data, data[0]))
        if msg == 'jograte-changed':
            self.jogRate = float(data[0])
        elif msg == 'jograte-angular-changed':
            self.jogRateAngular = float(data[0])
        elif msg == 'jogincrements-changed':
            self.jogIncrement = float(data[0][0])
        elif msg == 'jogincrement-angular-changed':
            self.jogIncremtAngular = float(data[0][0])
        elif msg == 'joint-selection-changed':
            self.activeJoint = int(data[0])
        elif msg == 'axis-selection-changed':
            print ('pre axis state', self.axesSelected,self.currentSelectedAxis)
            flag = 1
            if data[0] == 'MPG0':
                self.currentSelectedAxis = data[0]
                flag = 0
                self.axesSelected['MPG0'] = True
            else:
               self.axesSelected['MPG0'] = False
            for i in(self.INFO.AVAILABLE_AXES):
                if data[0] == i:
                    state = True
                    self.currentSelectedAxis = data[0]
                    flag = 0
                else:
                    state = False
                self.axesSelected[i] = int(state)

            if flag:
                self.currentSelectedAxis = 'None'
            print ('axis state', self.axesSelected,self.currentSelectedAxis)

    # send msg to hal_glib
    def writeMsg(self, msg, data):
        print('Write Msg called')
        if ZMQ:
            topic = self.writeTopic
            message = json.dumps({'FUNCTION':msg,'ARGS':data})
            LOG.debug('Sending ZMQ Message:{} {}'.format(topic, message))
            self.writeSocket.send_multipart(
                        [bytes(topic.encode('utf-8')),
                            bytes((message).encode('utf-8'))])

    def shutdown(self,signum=None,stack_frame=None):
        LOG.debug('shutdown')
        global app
        app.quit()

    def cycleStart(self):
        # cycle start
        self.writeMsg('request_cycle_start', True)

    def cyclePause(self):
        self.writeMsg('request_cycle_pause', True)

    def ok(self):
        self.writeMsg('request_ok', True)

    def cancel(self):
        self.writeMsg('request_cancel', True)

    def getMdiName(self, num):
        if num >len(self.INFO.MDI_COMMAND_DICT)-1:
            return 'None'
        temp = list(self.INFO.MDI_COMMAND_DICT.keys())[num]
        LOG.debug('{} {}'.format(num,temp))
        return temp

    def getMacroNames(self):
        for i in self.INFO.INI_MACROS:
            name = i.split()[0]
            LOG.debug('{} {}'.format(name,i))

    def runIndexedMacro(self, num):
        name = self.getMdiName(num)
        LOG.debug('Macro name:{} ,index: {}'.format(name, num))
        if name != 'None':
            self.writeMsg('request_macro_call', name)

    def getMdiCount(self):
        print(len(self.INFO.MDI_COMMAND_DICT))
        return len(self.INFO.MDI_COMMAND_DICT)

    def getJogRate(self):
        return self.jogRate
    def setJogRate(self, value):
        self.writeMsg('set_jograte', value)

    def getJogRateAngular(self):
        return self.jogRateAngular
    def setJogRateAngular(self, value):
        self.writeMsg('set_jograte_angular', value)

    def getSelectedAxis(self):
        name = self.currentSelectedAxis
        if name == 'None':
            index = -1
        elif name =='MPG0':
            index = 100
        else:
            index = 'XYZABCUVW'.index(name)
        return index 
    def setSelectedAxis(self, value):
        if value < 0:
            letter = 'None'
        elif value == 100:
            letter = 'MPG0'
        else:
            letter ='XYZABCUVW'[value]
        self.writeMsg('set_selected_axis', letter)

    def isAxisSelected(self, index):
        if index == 100:
            letter = 'MPG0'
        else:
            letter = 'XYZABCUVW'[index]
        return int(self.axesSelected[letter])

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
