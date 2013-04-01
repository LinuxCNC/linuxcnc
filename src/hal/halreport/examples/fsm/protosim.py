import zmq
import time,os,sys,random
from fysom import Fysom
try:
    import cPickle as pickle
except:
    import pickle
import pprint
import traceback
import optparse
from threading import Thread

# sudo easy_install cmd2
from cmd2 import Cmd, make_option, options

pp = pprint.PrettyPrinter(indent=4)

peers = ['foo','bar']
#peers = ['foo','bar','baz']
me = sys.argv[1]  # Peer identity
del sys.argv[1]   # this confuses cmd2, so delete
peers.remove(me)  # FSM only for 'the rest of them'

random.seed(os.getpid())
my_sid = random.randint(1, 3)
sids_heard = []

debug = 0
done = 0
port = 5555
interface = "127.0.0.1"
#interface = "eth1"

rx_uri = "epgm://%s;239.192.1.1:%d" % (interface, port)
tx_uri = "epgm://%s;239.192.1.1:%d" % (interface, port)

timeout = 200.0 # ms - the 'timer jiffy'

context = zmq.Context()
tx = context.socket(zmq.PUB)
tx.connect(tx_uri)

rx = context.socket(zmq.SUB)
rx.bind(rx_uri)
rx.setsockopt(zmq.SUBSCRIBE, "")

def printstatechange(e):
    print 'STATECHANGE fsm %s origin=%s event=%s %s => %s' % (e.fsm.ident,e.origin,e.event, e.src, e.dst)

def printtransition(e):
    pass
#print 'TRANSITION fsm %s origin=%s event=%s %s => %s' % (e.fsm.ident,e.origin,e.event, e.src, e.dst)

def on_holddown_expired(t):
    m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : sids_heard }
    tx.send_multipart(('', pickle.dumps(m)))

def onbeforehello(e):
    global sids_heard, my_sid
    #print "rx HELLO: %s %s => %s  me=%d heard=%s " % (e.event,e.src,e.dst, my_sid, str(sids_heard))
    return True

def onenterwaiting(e):
    #m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : [1,2,3] }
    m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : sids_heard }
    print "onenterwaiting: sending",m
    tx.send_multipart(('', pickle.dumps(m)))

def onenter1_way(e):
    #m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : [1,2,3] }
    m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : sids_heard }
    print "onenter1_way: sending",m
    tx.send_multipart(('', pickle.dumps(m)))

def onenter2_way(e):
    #m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : [1,2,3] }
    m = { 'peer' : me, 'msg' : 'hello', 'lsid' : my_sid , 'seen' : sids_heard }
    print "onenter2_way: sending",m
    tx.send_multipart(('', pickle.dumps(m)))
    e.fsm.timer_start('keepalive')

# the peer-peer proto FSM's

protospec = {
  'initial': 'down',
  'events': [

    # open link, start hello timer, send hello message
    {'name': 'startup',         'src': 'down',     'dst': 'waiting'},

    # a hello message was received and our lsid is seen
    {'name': 'hello_seenme',    'src': 'waiting',  'dst': '2_way' },

    # a hello message was received and our lsid is NOT seen
    {'name': 'hello',           'src': 'waiting',  'dst': '1_way'},
    {'name': 'hello',           'src': '2_way',  'dst': '1_way'},
    {'name': 'hello',           'src': '1_way',  'dst': 'waiting'},

    # a hello message was received and our lsid is seen
    {'name': 'hello_seenme',    'src': '1_way',  'dst': '2_way' },

    {'name': 'hello_seenme',    'src': '2_way',  'dst': '2_way' },


    {'name': 'keepalive_expired',  'src': '2_way',  'dst': '1_way'},


    #    {'name': 'hello',           'src': 'waiting',  'dst': '1_way'},



    # send a hel
    {'name': 'holddown_expired','src': 'down',     'dst': 'down'},
    {'name': 'ev2',             'src': 'waiting',  'dst': 'down'},
    {'name': 'ev3',             'src': 'waiting',  'dst': '1_way'},
    {'name': 'ev4',             'src': '1_way',    'dst': 'waiting'},
    {'name': 'ev5',             'src': 'waiting',  'dst': '2_way'},
    {'name': 'ev6',             'src': '2_way',    'dst': 'waiting'},
    {'name': 'ev7',             'src': '1_way',    'dst': '2_way'},
    {'name': 'ev8',             'src': '2_way',    'dst': '1_way'},
    {'name': 'ev9',             'src': '1_way',    'dst': 'down'},
    {'name': 'ev10',            'src': '2_way',    'dst': 'down'},
  ]
  ,
  'timers' : [
    {'name': 'holddown',  'duration' : 3.0 },
    {'name': 'keepalive', 'duration' : 5.0 },
    ]
  ,
  'callbacks': {
      'ontimerholddown_expired' : on_holddown_expired,
      'onbeforehello' :  onbeforehello,
      'onbeforehello_seenme' :  onbeforehello,
      'onenterwaiting' : onenterwaiting,
      'onenter1_way' : onenter1_way,
      'onenter2_way' : onenter2_way,
   }
}
protofsm = dict()

#  proto FSM for each peer
for p in peers:
    fsm = Fysom(protospec)
    fsm.onchangestate = printstatechange
    fsm.ontransition = printtransition
    fsm.ident = p
    fsm.timer = -1.0 # < 0: not armed
    fsm.startup(origin='initialisation')
    protofsm[p] = fsm

def cause(fd, event,peer,origin, warn, msg):
    if not peer in peers: return
    fsm = protofsm[peer]
    if fsm.can(event):
        result = getattr(fsm, event)(origin=origin, msg=msg)
    else:
        if warn: fd.write("%s: event %s not applicable in state %s (origin: %s)\n" %
                          (peer,event,fsm.current, origin))

class Cli(Cmd):

    def do_status(self, arg, opts=None):
        for peer,fsm in protofsm.items():
            self.stdout.write("%s: state %s\n" % (peer, fsm.current))

    def do_timer(self, arg, opts=None):
        protofsm[arg].timer_print()

    def do_start(self, arg, opts=None):
        (peer,timer) = arg.split()
        protofsm[peer].timer_start(timer)

    def do_stop(self, arg, opts=None):
        (peer,timer) = arg.split()
        protofsm[peer].timer_stop(timer)

    def do_event(self, arg, opts=None):
        ''' event peer eventname'''
        try:
            (peer,event) = arg.split()
            cause(self.stdout, event, peer, 'cmd', True, msg = {})
        except Exception,e:
            self.stdout.write("Exception: %s\n" % (e))
            traceback.print_exc(file=sys.stdout)
            self.stdout.write("Usage: 'event peer eventtype'\n")

class CmdInterp(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.c = Cli()

    def run(self):
        global done
        self.c.cmdloop()
        done = 1

# start the command interpreter
ci  = CmdInterp()
ci.setDaemon(True)
ci.start()


# Process messages from rx socket, timeout, cmd
poller = zmq.Poller()
poller.register(rx, zmq.POLLIN)
while not done:
    socks = dict(poller.poll(timeout=timeout))

    if rx in socks and socks[rx] == zmq.POLLIN:
        mp = rx.recv_multipart()
        #print "got: ",mp
        if len(mp) != 2:
            continue
        m = pickle.loads(mp[1])

        if not m['peer'] in peers: continue

        if m['msg'] == 'hello':  # dont have ND FSM - hide in event name
            reported_sids = m.get('seen',[])
            sids_heard= list(set(sids_heard)|set(reported_sids))
            if m['lsid'] not in sids_heard:
                sids_heard += [m['lsid']]
            if my_sid in reported_sids:
                m['msg'] = 'hello_seenme'

        cause(sys.stdout,m['msg'], m['peer'], 'net', True, m)

    if not socks:
        for p in peers:
            #m = { 'peer' : me, 'event' : 'clear' }
            #tx.send_multipart(('', pickle.dumps(m)))
            #cause(sys.stdout,'timeout' , p, 'timer', False)
            protofsm[p].jiffie(timeout/1000.0)
