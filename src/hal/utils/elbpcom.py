#!/usr/bin/env python3
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
    help="IP address of board (default: 192.168.1.121)",
    metavar="X.Y.Z.W", default="192.168.1.121")
parser.add_option("-p", "--port", dest="sport",
    help="UDP port of board (default: 27181)", type="int", default=27181)
parser.add_option("-t", "--timeout", dest="timeout",
    help="Response timeout in seconds (default: 0.2)", type="float", default=.2)
parser.add_option("-s", "--space", dest="space", default=0,
    choices = ["0", "1", "2", "3", "4", "5", "6", "7"],
    help="Address space to read or write (default: 0, the hm2 register file)")
parser.add_option("--info", dest="info", action="store_true",
    default=False,
    help="Select info area for read operation (default: memory space)")
parser.add_option("-a", "--address", type="int", dest="address", default=None,
    help="Base address to read or write")
parser.add_option("-I", "--increment", dest="increment", action="store_true",
    default=True,
    help="auto-increment address (enabled by default, use --no-increment to disable)")
parser.add_option("-n", "--no-increment", dest="increment",
    action="store_false",
    help="do not auto-increment address")
parser.add_option("--size", dest="size",
    help="Transfer size in number of bytes (default: look up preferred transfer size in the space's info area)",
    type="int", default=0)
parser.add_option("-r", "--read", type="int", dest="read", default=None,
    help="Number of bytes to read")
parser.add_option("-w", "--write", type="string", dest="write", default=None,
    help="Hex-coded Values to read")

parser.add_option("--read-info", dest="read_info", default=False,
    action="store_true",
    help="Read and decode the info area of the selected memory space.")

options, args = parser.parse_args()
if options.space: options.space = int(options.space)

sizemap = {1: 0, 2: 1, 4: 2, 8: 3}

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
s.settimeout(options.timeout)

def transact(sdata:bytes, quiet=False, response=True):
    s.sendto(sdata, (options.sip, options.sport))
    if not response: return
    try:
        data, daddr = s.recvfrom(1280)
        if not quiet: print("<", data.hex())
        if not quiet: print("     ", re.sub('[^ -~]', '.', data.decode('utf-7', errors='replace')))
        return data
    except socket.timeout:
        if not quiet: print("! no response")
        return None

def interact():
    try:
        while 1:
            sdata = input("> ")
            if not sdata.strip(): break
            transact(sdata)
    except KeyboardInterrupt:
        pass

def make_read_request(space:int, info:bool, size:int, increment:bool, address:int, nbytes:int) -> bytes:
    return struct.pack("<HH",
        (1<<14) | (space << 10) | (info << 13)
        | (sizemap[size] << 8) | (increment << 7) | int(nbytes/size),
        address)

def make_write_request(space:int, info:bool, size:int, increment:bool, address:int, data:bytes) -> bytes:
    return struct.pack("<HH",
        (1<<15) | (1<<14) | (space << 10) | (info << 13)
        | (sizemap[size] << 8) | (increment << 7) | int(len(data) / size),
        address) + data

def optimal_size(space:int, info:bool, address:int, nbytes:int):
    if info: return 2
    info = transact(make_read_request(space, True, 2, True, 2, 4), quiet=True)
    if info is None:
        raise RuntimeError("Failed to get information about memory space %d" % space)
    memsizes, memranges = struct.unpack("<HH", info)
    maxaddr = 1 << (memranges & 0x3f)
    #print "# Note: space %d has maxaddr %d (memsizes=0x%x memranges=0x%x)" % (space, maxaddr, memsizes, memranges)
    # This gets the wrong limit with my firmware on space 6, so disabling it
    #if address + nbytes >= maxaddr: raise ValueError, "Address out of range (address=%d nbytes=%d maxaddr=%d)" % (address, nbytes, maxaddr)
    for i in (3,2,1,0):
        b = (1<<i)
        if address % b or nbytes % b or not memsizes & b: continue
        return b
    raise ValueError("Access size incompatible with address or length (address=%d nbytes=%d memsizes=%d)" % (address, nbytes, memsizes))

def get_uint16(data:bytes, offset:int):
    return (data[offset+1] << 8) | data[offset]

def print_info(data:bytes):
    print(f"info area for memory space {options.space}:")

    cookie = get_uint16(data, 0)
    print(f"    cookie: 0x{cookie:04x}")

    memsize = get_uint16(data, 2)
    memsize_writable = memsize & 0x8000
    memsize_type = (memsize & 0x7f00) >> 8
    memsize_access = memsize & 0x000f
    print(f"    memsize: 0x{memsize:04x}")

    print("        Writable" if memsize_writable else "        Read-Only")

    if (memsize_type == 0x01):
        print("        type=01 (Register)")
    elif (memsize_type == 0x02):
        print("        type=02 (Memory)")
    elif (memsize_type == 0x0e):
        print("        type=0e (EEPROM)")
    elif (memsize_type == 0x0f):
        print("        type=0f (Flash)")
    else:
        print(f"        type={memsize_type:02x}(unknown)")

    print(f"        access=0x{memsize_access:01x}", end='')
    if memsize_access & 0x1:
        print(" 8-bit", end='')
    if memsize_access & 0x2:
        print(" 16-bit", end='')
    if memsize_access & 0x4:
        print(" 32-bit", end='')
    if memsize_access & 0x8:
        print(" 64-bit", end='')
    print()

    memranges = get_uint16(data, 4)
    e = (memranges & 0xf800) >> 11
    p = (memranges & 0x07c0) >> 6
    s = memranges & 0x003f
    print(f"    memranges: 0x{memranges:04x}")
    print(f"        erase block size: {e} ({2**e} bytes)")
    print(f"        page size: {p} ({2**p} bytes)")
    print(f"        Ps address range: {s} ({2**s} bytes)")

    addr_ptr = get_uint16(data, 6)
    print(f"    addr ptr: 0x{addr_ptr:04x}")

    name = str(data[8:], encoding='utf-7')
    print(f"    name: {name}")


if options.read:
    if options.address is None: raise SystemExit("--read must specify --address")
    if options.size == 0:
        options.size = optimal_size(options.space, options.info, options.address, options.read if options.increment else 0)
    command = make_read_request(options.space, options.info, options.size, options.increment, options.address, options.read)
    print(">", command.hex())
    transact(command)

elif options.write:
    if options.address is None: raise SystemExit("--write must specify --address")
    write = bytes.fromhex(options.write)
    if options.size == 0:
        options.size = optimal_size(options.space, options.info, options.address, len(write) if options.increment else 0)
    command = make_write_request(options.space, options.info, options.size, options.increment, options.address, write)
    print(">", command.hex())
    transact(command, response=False)

elif options.read_info:
    if options.size == 0:
        options.size = optimal_size(options.space, True, 0x0000, 16)
    command = make_read_request(options.space, True, options.size, options.increment, 0x00, 0x10)
    print(">", command.hex())
    data = transact(command)
    print_info(data)

elif args:
    for a in args:
        transact(a)
else:
    interact()
