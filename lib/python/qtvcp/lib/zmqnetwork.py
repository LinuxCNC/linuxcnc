from PyQt5 import QtCore

from qtvcp import logger
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

####################
# initialize
####################

# only import zmq if needed and present
def import_ZMQ():
    try:
        import zmq
    except:
        LOG.warning('Problem importing zmq - Is python3-zmq installed?')
        # nope no messaging for you
        return False
    else:
        import json
        # since we imported in a function we need to globalize
        # the libraries so screenoptions can reference them.
        global zmq
        global json
        # imports are good - go ahead and setup messaging
        return True

class ZMQSendReceive():
    def __init__(self, instance = None, topic = b""):
        self._inst = instance
        self.add_send_zmq = False
        self.add_receive_zmq = True

        self._zmq_sub_subscribe_name = topic
        self._zmq_sub_socket_address = "tcp://127.0.0.1:5690"
        self._zmq_pub_socket_address = "tcp://127.0.0.1:5690"

    def openPublish(self):
        self.add_send_zmq = True
        self.init_zmq_publish()

    def openSubscribe(self):
        self.add_receive_zmq = True
        self.init_zmq_subscribe()

######################################
# subscribe
######################################

    def init_zmq_subscribe(self):
        if import_ZMQ():
            try:
                self._zmq_sub_context = zmq.Context()
                self._zmq_sub_sock = self._zmq_sub_context.socket(zmq.SUB)
                self._zmq_sub_sock.connect(self._zmq_sub_socket_address)
                self._zmq_sub_sock.setsockopt(zmq.SUBSCRIBE, self._zmq_sub_subscribe_name)

                self.read_noti = QtCore.QSocketNotifier(
                                    self._zmq_sub_sock.getsockopt(zmq.FD),
                                    QtCore.QSocketNotifier.Read, None)
                self.read_noti.activated.connect(self.on_read_msg)
            except Exception as e:
                LOG.error('zmq subscribe to message setup error: {}'.format(e))

    def on_read_msg(self):
        self.read_noti.setEnabled(False)
        if self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLIN:
            while self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLIN:
                # get raw message
                topic, data = self._zmq_sub_sock.recv_multipart()
                # convert from json object to python object
                y = json.loads(data)
                # get the function name
                function = y.get('FUNCTION')
                # get the arguments
                arguments = y.get('ARGS')
                LOG.debug('{} Sent ZMQ Message:{} {}'.format(topic,function,arguments))

                # call handler function with arguments
                try:
                      self.converToFunction(channel, msg)
                except Exception as e:
                    LOG.error('zmq message parcing error: {}'.format(e))
                    LOG.error('{} {}'.format(function, arguments))
        elif self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLOUT:
            print("[Socket] zmq.POLLOUT")
        elif self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLERR:
            print("[Socket] zmq.POLLERR")
        self.read_noti.setEnabled(True)

    # convert message to a function call
    def converToFunction(self, topic, data):
        # convert from json object to python object
        y = json.loads(data)
        # get the function name
        function = y.get('FUNCTION')
        # get the arguments
        arguments = y.get('ARGS')
        if self._inst is None:
            print('{} Sent ZMQ Message:{} {}'.format(topic,function,arguments))
            return

        # call handler function with arguments
        try:
            # call self._inst (a function) with all arguments
            if callable(self._inst):
                self._inst(topic,function,arguments)
            else:
                # directly call the function of self._inst (a class instance)
                self._inst[function](arguments)
        except Exception as e:
            LOG.error('zmq message parsing error: {}'.format(e))
            LOG.error('{} {}'.format(function, arguments))


################################
# Publish
################################

    def init_zmq_publish(self):
        if import_ZMQ():
            try:
                self._zmq_pub_context = zmq.Context()
                self._zmq_pub_socket = self._zmq_pub_context.socket(zmq.PUB)
                self._zmq_pub_socket.bind(self._zmq_pub_socket_address)
            except Exception as e:
                LOG.error('zmq publish message setup error: {}'.format(e))

    def zmq_write_message(self, args,topic = 'QtVCP'):
        if self.add_send_zmq:
            try:
                message = json.dumps(args)
                LOG.debug('Sending ZMQ Message:{} {}'.format(topic,message))
                self._zmq_pub_socket.send_multipart(
                    [bytes(topic.encode('utf-8')),
                        bytes((message).encode('utf-8'))])
            except Exception as e:
                LOG.error('zmq message sending error: {}'.format(e))
        else:
            LOG.info('ZMQ Message not enabled. message:{} {}'.format(topic,args))

if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)

    m = ZMQSendReceive()
    m.init_zmq_subscribe()

    sys.exit(app.exec_())

