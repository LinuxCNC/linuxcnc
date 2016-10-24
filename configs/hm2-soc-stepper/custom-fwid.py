# to create a custom fwid message, edit as needed
# python custom-fwid.py >custom.bin

# pass to hm2_soc_ol like so:
# loadrt hm2_soc_ol descriptor=custom.bin ..other args...


import sys
import os
import binascii
import struct
import google.protobuf.text_format
from machinetalk.protobuf.firmware_pb2 import Firmware, Connector
import subprocess

def get_git_revision_short_hash():
    return subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).strip()

maxsize = 2048
width = 32  # of MIF file
format = '<L'  # LittleEndian

# construct the descriptor object
fw  = Firmware()

try:
    fw.build_sha = get_git_revision_short_hash()
except Exception:
    fw.build_sha = "not a git repo"

try:
    fw.comment = os.getenv("BUILD_URL") # jenkins job id
except Exception:
    fw.comment = "$BUILD_URL unset"

fw.fpga_part_number = "altera socfpga"
fw.num_leds = 4
fw.board_name = "Terasic DE0-Nano"

c = fw.connector.add()
c.name = "gpio0.p2"
c.pins = 17

c = fw.connector.add()
c.name = "gpio0.p3"
c.pins = 17

c = fw.connector.add()
c.name = "gpio1.p2"
c.pins = 17

c = fw.connector.add()
c.name = "gpio1.p3"
c.pins = 17


# serialize it to a blob
blob = fw.SerializeToString()

assert len(blob) <= maxsize, ValueError("encoded message size too large: %d (max %d)" % (len(blob), maxsize))

sys.stdout.write(blob)
