#!/usr/bin/python

import sys
import struct

#print "FILE used: %s" % sys.argv[1:][0]

with open( sys.argv[1:][0], mode="rb") as f:
    globresult = "OK"
    word = f.read( 4)
    while word:
        opcode = struct.unpack('I',word)
        if opcode[0] != 0x79000000:
            word = f.read( 4)
            count = struct.unpack('I',word)
            print "OPCODE: 0x%08x, COUNT: %d" % (opcode[0], count[0]),
            result = "OK"
            for i in range(0,count[0]):
                word = struct.unpack('I',f.read( 4))
                if word != opcode:
#                    print "MISMATCH opcode 0x%08x differs from reference value 0x%08x" % (word[0],opcode[0]),
                    result = "FAIL"
                    globresult = "FAIL"
            print result
        word = f.read( 4)
    if globresult == "FAIL":
        sys.exit( 1)
