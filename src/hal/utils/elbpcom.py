#!/usr/bin/env python
# -*- coding: utf-8 -*-
#    Copyright 2012-2015
#    Peter C. Wallace <pcw@mesanet.com> and Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

import optparse
import re
import readline
import socket
import struct

parser = optparse.OptionParser("%prog [options] [commands]",
    description="Communicate with Mesa ethernet cards using the LBP16 protocol")
parser.add_option("-i", "--ip", dest="sip",
    help="IP address of board",
    metavar="X.Y.Z.W", default="192.168.1.121")
parser.add_option("-p", "--port", dest="sport",
    help="UDP port of board", type="int", default=27181)
parser.add_option("-t", "--timeout", dest="timeout",
    help="Response timeout in seconds", type="float", default=.2)
parser.add_option("-s", "--space", dest="space", default=None,
    choices = ["0", "1", "2", "3", "4", "5", "6", "7"],
    help="Address space to read or write")
parser.add_option("--info", dest="info", action="store_true",
    default=False,
    help="Select info area for read operation (default: memory space)")
parser.add_option("-a", "--address", type="int", dest="address", default=None,
    help="Base address to read or write")
parser.add_option("-I", "--increment", dest="increment", action="store_true",
    default=True,
    help="auto-increment address")
parser.add_option("-n", "--no-increment", dest="increment",
    action="store_false",
    help="do not auto-increment address")
parser.add_option("-r", "--read", type="int", dest="read", default=None,
    help="Number of bytes to read")
parser.add_option("-w", "--write", type="string", dest="write", default=None,
    help="Hex-coded Values to read")
options, args = parser.parse_args()
if options.space: options.space = int(options.space)

sizemap = {1: 0, 2: 1, 4: 2, 8: 3}

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
s.settimeout(options.timeout)

def transact(sdata, quiet=False, response=True):
    sdata = "".join(sdata.split()).decode("hex")
    s.sendto(sdata, (options.sip, options.sport))
    if not response: return
    try:
        data, daddr = s.recvfrom(1280)
        if not quiet: print "<", data.encode("hex")
        if not quiet: print "     ", re.sub('[^ -~]', '.', data)
        return data
    except socket.timeout:
        if not quiet: print "! no response"
        return None

def interact():
    try:
        while 1:
            sdata = raw_input("> ")
            if not sdata.strip(): break
            transact(sdata)
    except KeyboardInterrupt:
        pass

def make_read_request(space, info, size, increment, address, nbytes):
    return struct.pack("<HH",
        (1<<14) | (space << 10) | (info << 13)
        | (sizemap[size] << 8) | (increment << 7) | (nbytes/size),
        address)

def make_write_request(space, info, size, increment, address, bytes):
    return struct.pack("<HH",
        (1<<15) | (1<<14) | (space << 10) | (info << 13)
        | (sizemap[size] << 8) | (increment << 7) | (len(bytes) / size),
        address) + bytes

def optimal_size(space, info, address, nbytes):
    if info: return 2
    info = transact(make_read_request(space, True, 2, True, 2, 4).encode("hex"), quiet=True)
    if info is None:
        raise RuntimeError, "Failed to get information about memory space %d" % space
    memsizes, memranges = struct.unpack("<HH", info)
    maxaddr = 1 << (memranges & 0x3f)
    #print "# Note: space %d has maxaddr %d (memsizes=0x%x memranges=0x%x)" % (space, maxaddr, memsizes, memranges)
    # This gets the wrong limit with my firmware on space 6, so disabling it
    #if address + nbytes >= maxaddr: raise ValueError, "Address out of range (address=%d nbytes=%d maxaddr=%d)" % (address, nbytes, maxaddr)
    for i in (3,2,1,0):
        b = (1<<i)
        if address % b or nbytes % b or not memsizes & b: continue
        return b
    raise ValueError, "Access size incompatible with address or length (address=%d nbytes=%d memsizes=%d)" % (address, nbytes, memsizes)

if options.read:
    if options.space is None: raise SystemExit, "--read must specifiy --space"
    if options.address is None: raise SystemExit, "--read must specifiy --address"
    size = optimal_size(options.space, options.info, options.address, options.read if options.increment else 0)
    command = make_read_request(options.space, options.info, size, options.increment, options.address, options.read)
    command = command.encode("hex")
    print ">", command
    transact(command)

elif options.write:
    if options.space is None: raise SystemExit, "--write must specifiy --space"
    if options.address is None: raise SystemExit, "--write must specifiy --address"
    write = options.write.decode("hex")
    size = optimal_size(options.space, options.info, options.address, len(write) if options.increment else 0)
    command = make_write_request(options.space, options.info, size, options.increment, options.address, write)
    command = command.encode("hex")
    print ">", command
    transact(command, response=False)

elif args:
    for a in args:
        transact(a)
else:
    interact()
