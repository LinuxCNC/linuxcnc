# HAL ring command/response example
#
# send a command message to HAL component
# receive a reply message from same
#
# message encodings - Protobuf and ASCII

import struct
import binascii

import ring
import time
from   types_pb2 import * #MT_COMMAND, MT_RESPONSE, MT_ASCII, PING, ON_COMPLETION
from   message_pb2 import Container, Command, Response

# message Command {
#     required CmdType              op = 10;
#     optional int32            serial = 20;
#     optional ReplyRequired    rsvp   = 30;
#     optional Originator       origin = 40;
#     repeated Object           args   = 50;
#     optional int32         timestamp = 60;
# }

# message Response {
#     required RespType        response = 5;
#     optional CmdType     in_reply_to = 10;
#     optional int32    request_serial = 20;
#     optional StatusType       status = 25;
#     optional Originator       origin = 30;
#     optional int64         timestamp = 40;
#     optional string         errormsg = 50;
#     repeated Object             args = 60;
#     repeated Object      failed_args = 70;
# }


class Timeout(Exception):
    pass

timeout = 1.0
interval = 0.1

# def send_text(ring, text):
#     wirefmt = struct.pack("!LHH", len(text), MT_DISPLAYABLE, ENC_ASCII_STRING)
#     wirefmt += text
#     ring.write(wirefmt, len(wirefmt))

c = ring.attach("command")
r = ring.attach("response")
c.writer = ring.comp_id

#send_text(c, "Hi there!")

for i in range(10):
    # construct a protobuf-encoded message
    container = Container()
    cmd = container.command

    #print dir(cmd)
    cmd.op = PING
    cmd.serial = i
    cmd.rsvp = ON_COMPLETION
    msg = cmd.SerializeToString()

    # encode the universal header
    # NB: network byte order
    #header = struct.pack("!LHH", len(msg), MT_COMMAND, ENC_PROTOBUF)

    # send it off
    c.write(msg,len(msg))

    # wait for response from RT component
    t = timeout
    while t > 0:
        n = r.next_size()
        if n > -1:
            b = r.next_buffer()
            # expecing a Response
            reply = Reply()
            reply.ParseFromString(b)
            print "reply:", str(reply)
            del reply
            r.shift()
            break
        time.sleep(interval)
        t -= interval
        if t < 0:
            raise Timeout

        #send_text(c, "Bye now.")
