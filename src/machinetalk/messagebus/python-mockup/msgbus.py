import zmq
import sys
import threading
import time

from optparse import OptionParser

parser = OptionParser()
parser.add_option("-v","--verbose", action="store_true", dest="verbose",
                  help="print actions as they happen")

(options, args) = parser.parse_args()

me = "msgbus"

# the command rail:
cmduri   = 'tcp://127.0.0.1:5571'
# the response rail:
responseuri = 'tcp://127.0.0.1:5573'


class MsgbusTask(threading.Thread):
    def __init__(self):
        threading.Thread.__init__ (self)
        self.kill_received = False

    def handle(self, socket, error, sname, presence):
        msg = socket.recv_multipart()
        if options.verbose: print "---%s %s recv: %s" % (me,sname,msg)
        if len(msg) == 1:
            frame = msg[0]
            sub = ord(frame[0])
            topic = frame[1:]

            if sub == 1:
                presence[topic] = True
                if options.verbose: print "--- %s %s subscribe: %s" % (me,sname,topic)
            elif sub == 0:
                if options.verbose: print "--- %s %s unsubscribe: %s" % (me,sname,topic)
                del presence[topic]
            else:
                if options.verbose: print "---%s %s recv: invalid frame: %s" % (me,sname,msg)

        else:
            dest = msg[1]
            if dest in presence:
                msg[0], msg[1] = msg[1], msg[0]
                socket.send_multipart(msg)
            else:
                # bad command destination only: reply with an error message
                # an unroutable response will be logged and dropped
                if error:
                    error.send_multipart([msg[0], "--- no destination: " + dest])
                if options.verbose: print "no %s destination: %s" % (sname,dest)


    def run(self):
        print('Msbus started')

        cmdsubs = dict()
        responsesubs = dict()
        context = zmq.Context()

        cmd = context.socket(zmq.XPUB)
        cmd.set(zmq.XPUB_VERBOSE,1)
        cmd.bind(cmduri)

        response = context.socket(zmq.XPUB)
        response.set(zmq.XPUB_VERBOSE,1)
        response.bind(responseuri)

        poll = zmq.Poller()
        poll.register(cmd,     zmq.POLLIN)
        poll.register(response,zmq.POLLIN)


        while not self.kill_received:
            s = dict(poll.poll(1000))
            if cmd in s:
                self.handle(cmd, response, "cmd", cmdsubs)
            elif response in s:
                self.handle(response, None, "response", responsesubs)
            else:
                continue

        context.destroy(linger=0)
        print('Msbus exited')

def main():
    try:
        bus = MsgbusTask()
        bus.start()
        while True: time.sleep(100)
    except (KeyboardInterrupt, SystemExit):
        bus.kill_received = True
        bus.join()

if __name__ == "__main__":
    main()
