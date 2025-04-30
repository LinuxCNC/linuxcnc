#!/usr/bin/env python
#
# Build hm2_modbus control command binary data file
# Copyright (C) 2025 B. Stultiens
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

#
# XML format
# <mesamodbus [comm param override attributes]>
#   <devices>
#     <device address="0x01" name="mydev"><description>My device</description></device>
#     ...
#   </devices>
#   <initlist>
#     <command device="mydev" function="6" address="0x0034" /><data val="0x0001" /><data... /></command>
#     <command delay="2000" />
#     ...
#   </initlist>
#   <commands>
#     <command device="mydev" function="R_INPUTS" address="0x0000" count="8" name="state"></command>
#     <command device="mydev" function="W_COILS" address="0x0000" count="8" name="relay">
#       <pin name="bulb-light"><description>Let there be light!</description></pin>
#       <pin name="shinyshiny" />
#       <pin name="vroooom" />
#       ...
#     </command>
#     <command delay="2000"><description>Use delay with care.</description></command>
#      ...
#   </commands>
# </mesamodbus>
#
# The <description> tag is just for comments. And, a future graphical interface
# may use it to make stuff more explanatory.
#
# See man mesambccc(1) for format details.
#

import sys
import os
import math
import getopt
import xml.etree.ElementTree as ET
import xml.parsers.expat as XP
import re
import struct

# HAL names are a bit picky
# The following allows, for example, device names "xyz" but also "xyz.0" and
# "xyz.1". It allows for multiple identical devices on different IDs and pin
# hierarchies.
pinpattern  = re.compile("^[a-z][a-z0-9-.]*[a-z0-9]*$")

devices  = None    # The list of devices

inputfilename = "unknown-filename" # file being processed
verbose = False
errorflag = False

# XXX: keep in sync with hm2_modbus.h
MAXDELAY     = 60000000   # Max one minute delay between commands (in microseconds)
MAXDELAYBITS = 1000000    # Max one Mega bit-times delay
MAXPINNAME   = 32         # Max chars for a name
MAXINTERVAL  = 3600000000 # Max one hour command interval (in microseconds)

# XXX: Keep in sync with hm2_modbus.h
MBCCB_FORMAT_PARITYEN_BIT  = 0 # bits 0 Enable parity if set
MBCCB_FORMAT_PARITYODD_BIT = 1 # bits 1 Odd parity if set
MBCCB_FORMAT_STOPBITS2_BIT = 2 # bit  2 0=8x1 1=8x2
MBCCB_FORMAT_DUPLEX_BIT    = 3 # bit  3 Set for full-duplex (rx-mask off)
MBCCB_FORMAT_SUSPEND_BIT   = 4 # bit  4 Set if state-machine starts suspended
MBCCB_FORMAT_PARITYEN      = (1 << MBCCB_FORMAT_PARITYEN_BIT)
MBCCB_FORMAT_PARITYODD     = (1 << MBCCB_FORMAT_PARITYODD_BIT)
MBCCB_FORMAT_STOPBITS2     = (1 << MBCCB_FORMAT_STOPBITS2_BIT)
MBCCB_FORMAT_DUPLEX        = (1 << MBCCB_FORMAT_DUPLEX_BIT)
MBCCB_FORMAT_SUSPEND       = (1 << MBCCB_FORMAT_SUSPEND_BIT)

# Values for parity internally
PARITY_NONE = 0
PARITY_ODD  = 1
PARITY_EVEN = 2

# Default parameters for serial communication.
# These can be overridden in the root tag as attributes.
CONFIGDEFAULT= {'baudrate'  : '9600',
                'parity'    : 'E',      # Translates into N=0, O=1, E=2
                'stopbits'  : '1',
                'duplex'    : 'HALF',
                'rxdelay'   : 'AUTO',
                'txdelay'   : 'AUTO',
                'drivedelay': 'AUTO',
                'icdelay'   : 'AUTO',
                'interval'  : '0',
                'suspend'   : '0',
                'writeflush': '1',
                'timeout'   : 'AUTO' }

configparams = CONFIGDEFAULT

# Allowed values for the parity attribute
PARITIES = {'N':    '0', 'O':   '1', 'E':    '2',
            'NONE': '0', 'ODD': '1', 'EVEN': '2',
            '0':    '0', '1':   '1', '2':    '2' }

DUPLEXES = {'HALF': '0', '0': '0',
            'FULL': '1', '1': '1'}

CONFIGLIMITS = {'baudrate'  : [1200, 1000000],
                'parity'    : [0, 2],
                'stopbits'  : [1, 2],
                'duplex'    : [0, 1],   # half/full
                'rxdelay'   : [0, 1020],
                'txdelay'   : [0, 1020],
                'drivedelay': [0, 31],
                'icdelay'   : [0, 255], # 0 signals auto
                'interval'  : [0, MAXINTERVAL],  # As-fast-as possible to ... seconds
                'suspend'   : [0, 1],   # Set to start suspended
                'writeflush': [0, 1],
                'timeout'   : [10000, 10000000] } # 10 milliseconds to 10 seconds (can override in <command>)

# XXX: Keep in sync with hal.h
HAL_BIT = 1
HAL_FLT = 2 # HAL_FLOAT
HAL_S32 = 3
HAL_U32 = 4
HAL_PRT = 5 # HAL_PORT, unused
HAL_S64 = 6
HAL_U64 = 7

# XXX: Keep in sync with hm2_modbus.c
MBT_AB       = 0x00
MBT_BA       = 0x01
MBT_ABCD     = 0x02
MBT_BADC     = 0x03
MBT_CDAB     = 0x04
MBT_DCBA     = 0x05
MBT_ABCDEFGH = 0x06
MBT_BADCFEHG = 0x07
MBT_CDABGHEF = 0x08
MBT_DCBAHGFE = 0x09
MBT_EFGHABCD = 0x0a
MBT_FEHGBADC = 0x0b
MBT_GHEFCDAB = 0x0c
MBT_HGFEDCBA = 0x0d
MBTBYTESIZES = [2, 2, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8, 8, 2, 2] # 16 entries

MBT_U        = 0x00
MBT_S        = 0x10
MBT_F        = 0x20

MBT_X_MASK   = 0x0f
MBT_T_MASK   = 0xf0

def mbtOrder(mtype):
    return mtype & MBT_X_MASK

def mbtType(mtype):
    return mtype & MBT_T_MASK

U_AB   = MBT_U | MBT_AB
U_BA   = MBT_U | MBT_BA
S_AB   = MBT_S | MBT_AB
S_BA   = MBT_S | MBT_BA
F_AB   = MBT_F | MBT_AB
F_BA   = MBT_F | MBT_BA
U_ABCD = MBT_U | MBT_ABCD
U_BADC = MBT_U | MBT_BADC
U_CDAB = MBT_U | MBT_CDAB
U_DCBA = MBT_U | MBT_DCBA
S_ABCD = MBT_S | MBT_ABCD
S_BADC = MBT_S | MBT_BADC
S_CDAB = MBT_S | MBT_CDAB
S_DCBA = MBT_S | MBT_DCBA
F_ABCD = MBT_F | MBT_ABCD
F_BADC = MBT_F | MBT_BADC
F_CDAB = MBT_F | MBT_CDAB
F_DCBA = MBT_F | MBT_DCBA

# Possible values of the 'haltype' attribute
HALTYPES = { 'HAL_BIT': HAL_BIT, 'HAL_FLOAT': HAL_FLT, 'HAL_FLT': HAL_FLT,
             'HAL_S32': HAL_S32, 'HAL_U32':   HAL_U32,
             'HAL_S64': HAL_S64, 'HAL_U64':   HAL_U64,
            # Be nice, allow types without useless prefix
             'BIT':     HAL_BIT, 'FLOAT':     HAL_FLT, 'FLT':     HAL_FLT,
             'S32':     HAL_S32, 'U32':       HAL_U32,
             'S64':     HAL_S64, 'U64':       HAL_U64 }

# Reverse map of HALTYPES
HALNAMES = { HAL_BIT: 'HAL_BIT', HAL_FLT: 'HAL_FLOAT',
             HAL_S32: 'HAL_S32', HAL_U32: 'HAL_U32',
             HAL_S64: 'HAL_S64', HAL_U64: 'HAL_U64' }

# Possible values of the 'modbustype' attribute
# [typeId, maxCount, nWords]
MBTYPES = { 'S_AB':       [MBT_S | MBT_AB,      125, 1], 'S_BA':       [MBT_S | MBT_BA,      125, 1],
            'U_AB':       [MBT_U | MBT_AB,      125, 1], 'U_BA':       [MBT_U | MBT_BA,      125, 1],
            'F_AB':       [MBT_F | MBT_AB,      125, 1], 'F_BA':       [MBT_F | MBT_BA,      125, 1],
            'S_ABCD':     [MBT_S | MBT_ABCD,     62, 2], 'S_BADC':     [MBT_S | MBT_BADC,     62, 2],
            'S_CDAB':     [MBT_S | MBT_CDAB,     62, 2], 'S_DCBA':     [MBT_S | MBT_DCBA,     62, 2],
            'U_ABCD':     [MBT_U | MBT_ABCD,     62, 2], 'U_BADC':     [MBT_U | MBT_BADC,     62, 2],
            'U_CDAB':     [MBT_U | MBT_CDAB,     62, 2], 'U_DCBA':     [MBT_U | MBT_DCBA,     62, 2],
            'F_ABCD':     [MBT_F | MBT_ABCD,     62, 2], 'F_BADC':     [MBT_F | MBT_BADC,     62, 2],
            'F_CDAB':     [MBT_F | MBT_CDAB,     62, 2], 'F_DCBA':     [MBT_F | MBT_DCBA,     62, 2],
            'S_ABCDEFGH': [MBT_S | MBT_ABCDEFGH, 31, 4], 'S_BADCFEHG': [MBT_S | MBT_BADCFEHG, 31, 4],
            'S_CDABGHEF': [MBT_S | MBT_CDABGHEF, 31, 4], 'S_DCBAHGFE': [MBT_S | MBT_DCBAHGFE, 31, 4],
            'S_EFGHABCD': [MBT_S | MBT_EFGHABCD, 31, 4], 'S_FEHGBADC': [MBT_S | MBT_FEHGBADC, 31, 4],
            'S_GHEFCDAB': [MBT_S | MBT_GHEFCDAB, 31, 4], 'S_HGFEDCBA': [MBT_S | MBT_HGFEDCBA, 31, 4],
            'U_ABCDEFGH': [MBT_U | MBT_ABCDEFGH, 31, 4], 'U_BADCFEHG': [MBT_U | MBT_BADCFEHG, 31, 4],
            'U_CDABGHEF': [MBT_U | MBT_CDABGHEF, 31, 4], 'U_DCBAHGFE': [MBT_U | MBT_DCBAHGFE, 31, 4],
            'U_EFGHABCD': [MBT_U | MBT_EFGHABCD, 31, 4], 'U_FEHGBADC': [MBT_U | MBT_FEHGBADC, 31, 4],
            'U_GHEFCDAB': [MBT_U | MBT_GHEFCDAB, 31, 4], 'U_HGFEDCBA': [MBT_U | MBT_HGFEDCBA, 31, 4],
            'F_ABCDEFGH': [MBT_F | MBT_ABCDEFGH, 31, 4], 'F_BADCFEHG': [MBT_F | MBT_BADCFEHG, 31, 4],
            'F_CDABGHEF': [MBT_F | MBT_CDABGHEF, 31, 4], 'F_DCBAHGFE': [MBT_F | MBT_DCBAHGFE, 31, 4],
            'F_EFGHABCD': [MBT_F | MBT_EFGHABCD, 31, 4], 'F_FEHGBADC': [MBT_F | MBT_FEHGBADC, 31, 4],
            'F_GHEFCDAB': [MBT_F | MBT_GHEFCDAB, 31, 4], 'F_HGFEDCBA': [MBT_F | MBT_HGFEDCBA, 31, 4] }

# Reverse map of MBTYPES
MBNAMES = { MBT_S | MBT_AB:       'S_AB',       MBT_S | MBT_BA:       'S_BA',
            MBT_U | MBT_AB:       'U_AB',       MBT_U | MBT_BA:       'U_BA',
            MBT_F | MBT_AB:       'F_AB',       MBT_F | MBT_BA:       'F_BA',
            MBT_S | MBT_ABCD:     'S_ABCD',     MBT_S | MBT_BADC:     'S_BADC',
            MBT_S | MBT_CDAB:     'S_CDAB',     MBT_S | MBT_DCBA:     'S_DCBA',
            MBT_U | MBT_ABCD:     'U_ABCD',     MBT_U | MBT_BADC:     'U_BADC',
            MBT_U | MBT_CDAB:     'U_CDAB',     MBT_U | MBT_DCBA:     'U_DCBA',
            MBT_F | MBT_ABCD:     'F_ABCD',     MBT_F | MBT_BADC:     'F_BADC',
            MBT_F | MBT_CDAB:     'F_CDAB',     MBT_F | MBT_DCBA:     'F_DCBA',
            MBT_S | MBT_ABCDEFGH: 'S_ABCDEFGH', MBT_S | MBT_BADCFEHG: 'S_BADCFEHG',
            MBT_S | MBT_CDABGHEF: 'S_CDABGHEF', MBT_S | MBT_DCBAHGFE: 'S_DCBAHGFE',
            MBT_S | MBT_EFGHABCD: 'S_EFGHABCD', MBT_S | MBT_FEHGBADC: 'S_FEHGBADC',
            MBT_S | MBT_GHEFCDAB: 'S_GHEFCDAB', MBT_S | MBT_HGFEDCBA: 'S_HGFEDCBA',
            MBT_U | MBT_ABCDEFGH: 'U_ABCDEFGH', MBT_U | MBT_BADCFEHG: 'U_BADCFEHG',
            MBT_U | MBT_CDABGHEF: 'U_CDABGHEF', MBT_U | MBT_DCBAHGFE: 'U_DCBAHGFE',
            MBT_U | MBT_EFGHABCD: 'U_EFGHABCD', MBT_U | MBT_FEHGBADC: 'U_FEHGBADC',
            MBT_U | MBT_GHEFCDAB: 'U_GHEFCDAB', MBT_U | MBT_HGFEDCBA: 'U_HGFEDCBA',
            MBT_F | MBT_ABCDEFGH: 'F_ABCDEFGH', MBT_F | MBT_BADCFEHG: 'F_BADCFEHG',
            MBT_F | MBT_CDABGHEF: 'F_CDABGHEF', MBT_F | MBT_DCBAHGFE: 'F_DCBAHGFE',
            MBT_F | MBT_EFGHABCD: 'F_EFGHABCD', MBT_F | MBT_FEHGBADC: 'F_FEHGBADC',
            MBT_F | MBT_GHEFCDAB: 'F_GHEFCDAB', MBT_F | MBT_HGFEDCBA: 'F_HGFEDCBA' }

R_COILS     =  1
R_INPUTS    =  2
R_REGISTERS =  3
R_INPUTREGS =  4
W_COIL      =  5
W_REGISTER  =  6
W_COILS     = 15
W_REGISTERS = 16

# Map function names/numbers to value
# [funcId, maxCount]
FUNCTIONS = { 'R_COILS'    : [R_COILS,    2000],  '1': [R_COILS,    2000],
              'R_INPUTS'   : [R_INPUTS,   2000],  '2': [R_INPUTS,   2000],
              'R_REGISTERS': [R_REGISTERS, 125],  '3': [R_REGISTERS, 125],
              'R_INPUTREGS': [R_INPUTREGS, 125],  '4': [R_INPUTREGS, 125],
              'W_COIL'     : [W_COIL,        1],  '5': [W_COIL,        1],
              'W_REGISTER' : [W_REGISTER,    1],  '6': [W_REGISTER,    1],
              'W_COILS'    : [W_COILS,    2000], '15': [W_COILS,    2000],
              'W_REGISTERS': [W_REGISTERS, 125], '16': [W_REGISTERS, 125] }

FUNCNAMES = { R_COILS: 'R_COILS', R_INPUTREGS: 'R_INPUTREGS', R_INPUTS: 'R_INPUTS', R_REGISTERS: 'R_REGISTERS',
              W_COIL:  'W_COIL',  W_REGISTER:  'W_REGISTER',  W_COILS:  'W_COILS',  W_REGISTERS: 'W_REGISTERS' }

WRITEFUNCTIONS = [ W_COIL, W_REGISTER, W_COILS, W_REGISTERS ]

# These functions always use HAL_BIT
# Also, the function is a register function if /not/ in this set
BITFUNCTIONS = [R_COILS, R_INPUTS, W_COIL, W_COILS]
REGFUNCTIONS = [R_REGISTERS, R_INPUTREGS, W_REGISTER, W_REGISTERS]

# Command flags for handling quirks and options
# XXX: keep in sync with hm2_modbus.h
MBCCB_PINF_SCALE    = 0x0001
MBCCB_PINF_CLAMP    = 0x0002
MBCCB_PINF_MASK     = 0x0003 # sum of pin flags

MBCCB_CMDF_TIMESOUT = 0x0001
MBCCB_CMDF_BCANSWER = 0x0002
MBCCB_CMDF_NOANSWER = 0x0004
MBCCB_CMDF_RESEND   = 0x0008
MBCCB_CMDF_WFLUSH   = 0x0010
MBCCB_CMDF_PARITYEN = 0x0100
MBCCB_CMDF_PARITYODD= 0x0200
MBCCB_CMDF_STOPBITS2= 0x0400
MBCCB_CMDF_INITMASK = 0x0707 # sum of allowed flags in init
MBCCB_CMDF_MASK     = 0x001f # sum of allowed normal command flags

# Allowed attributes in <mesamodbus>
MESAATTRIB = [ 'baudrate', 'drivedelay', 'duplex',   'icdelay', 'interval',
               'parity',   'rxdelay',    'stopbits', 'suspend', 'timeout',
               'txdelay',  'writeflush' ]

# Allowed attributes in <commands>/<command>
CMDSATTRIB = [ 'address',     'bcanswer', 'clamp',   'count',    'delay',
               'device',      'function', 'haltype', 'interval', 'modbustype',
               'name',        'noanswer', 'resend',  'scale',    'timeout',
               'timeoutbits', 'timesout', 'writeflush' ]

# Allowed attributes in <commands>/<command>/<pin>
PINSATTRIB = [ 'clamp', 'name', 'haltype', 'modbustype', 'scale' ]
SKIPATTRIB = [ 'skip' ]

# Allowed attributes in <initlist>/<command>
INITATTRIB = [ 'address',  'bcanswer', 'count',   'delay',       'device',
               'function', 'noanswer', 'timeout', 'timeoutbits', 'timesout' ]

# Allowed attributes in <initlist>/<command>/<data>
DATAATTRIB = [ 'modbustype', 'value' ]

# Allowed attributes in <initlist><command> for comms change
RATEATTRIB = [ 'baudrate', 'drivedelay', 'icdelay', 'parity',
               'rxdelay',  'stopbits',   'txdelay' ]

# Allowed attributes in <*>/<command delay="...">
DELAYATTRIB = [ 'delay' ]

#
# Program invocation message
#
def usage():
    print("""Mesa Modbus control command compiler.
Build hm2_modbus binary control command file from XML source description.
Usage:
  mesambccc [-h] [-o outfile] infile.mbccs

Options:
  -h|--help              This message
  -o file|--output=file  Write output to 'file'. Defaults to 'mesamodbus.output.mbccb'
  -v|--verbose           Verbose messages
""")
    sys.exit(2)

#
# Print to stderr with prefix
#
def perr(*args, **kwargs):
    global inputfilename
    global errorflag
    print("{}: error: ".format(inputfilename), file=sys.stderr, end='')
    print(*args, file=sys.stderr, **kwargs)
    errorflag = True

def pwarn(*args, **kwargs):
    global inputfilename
    print("{}: warning: ".format(inputfilename), file=sys.stderr, end='')
    print(*args, file=sys.stderr, **kwargs)

#
# Parse an int
#
def checkInt(s, k):
    if k not in s:
        return None
    try:
        val = int(s[k], 0)
    except ValueError as err:
        perr("Invalid integer '{}'".format(s[k]))
        return None
    except TypeError as err:
        perr("Invalid integer value type '{}'".format(s[k]))
        return None
    return val

#
# Parse a float
#
def checkFlt(s, k):
    if k not in s:
        return None
    try:
        val = float(s[k])
    except ValueError as err:
        perr("Invalid float '{}'".format(s[k]))
        return None
    except OverflowError as err:
        perr("Invalid float range '{}'".format(s[k]))
        return None
    except TypeError as err:
        perr("Invalid float value type '{}'".format(s[k]))
        return None
    return val

#
# Mangle 16 bit bytesex
#
def mangle16(d, mtype):
    mtype = mbtOrder(mtype)
    if MBT_AB == mtype:
        return d;
    if MBT_BA == mtype:
        return d[::-1];
    sys.exit("Invalid mtype '{}' in mangle16".format(mtype))

#
# Mangle 32 bit bytesex
#
def mangle32(d, mtype):
    mtype = mbtOrder(mtype)
    if MBT_ABCD == mtype:
        return d         # Source was in big endian
    if MBT_DCBA == mtype:
        return d[::-1]   # Reverse is little endian
    if MBT_BADC == mtype:
        d[0], d[1], d[2], d[3] = d[1], d[0], d[3], d[2]
        return d
    if MBT_CDAB == mtype:
        d[0], d[1], d[2], d[3] = d[2], d[3], d[0], d[1]
        return d
    sys.exit("Invalid mtype '{}' in mangle32".format(mtype))

#
# Mangle 64 bit bytesex
#
def mangle64(d, mtype):
    mtype = mbtOrder(mtype)
    if MBT_ABCDEFGH == mtype:
        return d         # Source was in big endian
    if MBT_HGFEDCBA == mtype:
        return d[::-1]   # Reverse is little endian
    if MBT_BADCFEHG == mtype:
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] = d[1], d[0], d[3], d[2], d[5], d[4], d[7], d[6]
        return d
    if MBT_CDABGHEF == mtype:
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] = d[2], d[3], d[0], d[1], d[6], d[7], d[4], d[5]
        return d
    if MBT_DCBAHGFE == mtype:
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] = d[3], d[2], d[1], d[0], d[7], d[6], d[5], d[5]
        return d
    if MBT_EFGHABCD == mtype:
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] = d[4], d[5], d[6], d[7], d[0], d[1], d[2], d[3]
        return d
    if MBT_FEHGBADC == mtype:
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] = d[5], d[4], d[7], d[6], d[1], d[0], d[3], d[2]
        return d
    if MBT_GHEFCDAB == mtype:
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] = d[6], d[7], d[4], d[5], d[2], d[3], d[0], d[1]
        return d
    sys.exit("Invalid mtype '{}' in mangle64".format(mtype))

#
# Get a boolean value
#
def getBoolean(attrib, name):
    if name not in attrib:
        return None
    a = attrib[name].upper()
    if 'T' == a or 'TRUE' == a or '1' == a:
        return True
    if 'F' == a or 'FALSE' == a or '0' == a:
        return False
    perr("Expected boolean value in '{}' attribute, not '{}'".format(name, attrib[name]))
    return None

#
# Get 'auto' or '%d xxx' format
#
def getAutoFmt(val, suffix):
    if suffix:
        suffix = " " + suffix
    return "auto" if 0 == val else "{}{}".format(val, suffix)

#
# Create a list of flags
#
def cflagList(flags):
    if 0 == flags:
        return "<none>"
    l = { MBCCB_CMDF_TIMESOUT: 'timesout', MBCCB_CMDF_BCANSWER: 'bcanswer',
          MBCCB_CMDF_NOANSWER: 'noanswer', MBCCB_CMDF_RESEND:   'resend',
          MBCCB_CMDF_WFLUSH:   'writeflush' }
    return ','.join([l[v] for v in l.keys() if flags & v])

def pflagList(flags):
    if 0 == flags:
        return "<none>"
    l = { MBCCB_PINF_SCALE: 'scale', MBCCB_PINF_CLAMP: 'clamp' }
    return ','.join([l[v] for v in l.keys() if flags & v])

#
# Merge and check comms config from root element attributes
#
def verifyConfigParams(cfg):
    err = False
    # Make it case insensitive
    cfg = { k: v.upper() for k,v in cfg.items() }
    # Fixup parity
    if cfg['parity'] not in PARITIES:
        perr("Attribute 'parity' must be 'E', 'O' or 'N'.")
        err = True
        cfg['parity'] = 'E'
    cfg['parity'] = PARITIES[cfg['parity']]
    # Fixup duplex
    if cfg['duplex'] not in DUPLEXES:
        perr("Attribute 'duplex' must be 'full' or 'half'.")
        err = True
        cfg['parity'] = 'FULL'
    cfg['duplex'] = DUPLEXES[cfg['duplex']]

    if 'AUTO' == cfg['rxdelay']:
        cfg['rxdelay'] = '0'
    if 'AUTO' == cfg['txdelay']:
        cfg['txdelay'] = '0'

    # Auto drive-delay will set it according to stopbits
    if 'AUTO' == cfg['drivedelay']:
        cfg['drivedelay'] = '0'

    # Set inter-character delay to auto calculation in driver
    if 'AUTO' == cfg['icdelay']:
        cfg['icdelay'] = '0'

    # Fixup suspend boolean
    b = getBoolean(cfg, 'suspend')
    cfg['suspend'] = '1' if True == b else '0'

    # Fixup writeflush boolean
    b = getBoolean(cfg, 'writeflush')
    cfg['writeflush'] = '1' if True == b else '0'

    # Set timeout to zero for auto calculation
    if 'AUTO' == cfg['timeout']:
        cfg['timeout'] = '0'

    # convert to int and check against the min/max values
    for k, v in cfg.items():
        # convert to integer
        try:
            p = int(v, 0)
            # Test limits
            if (p < CONFIGLIMITS[k][0] or p > CONFIGLIMITS[k][1]) and not (k == 'timeout' and p == 0):
                perr("Attribute '{}' in <mesamodbus> must be between {} and {} (is set to {})"
                    .format(k, CONFIGLIMITS[k][0], CONFIGLIMITS[k][1], p))
                err = True
            cfg[k] = p
        except ValueError as e:
            perr("Attribute '{}' must be an integer number".format(k))
            err = True
        except TypeError as e:
            perr("Attribute '{}' has an invalid value type '{}'".format(k, v))
            err = True
    return cfg if not err else None

#
# Parse the <devices> tag content
#
def handleDevices(devs):
    devlist  = {'broadcast': 0}   # map device name to address
    addrlist = {0: 'broadcast'}   # map address to device name
    for dev in devs:
        ldl = "devices/device[{}]".format(len(devlist))
        # Attribute name must exist and of proper format
        if 'name' not in dev.attrib:
            perr("Missing 'name' attribute in {}".format(ldl))
            continue
        name = dev.attrib['name']
        if not pinpattern.match(name):
            perr("Attribute 'name' contains invalid characters {}".format(ldl))
            continue
        if len(name) > MAXPINNAME:
            perr("Attribute 'name' longer than {} characters in {}".format(MAXPINNAME, ldl))
            continue

        # Device address in byte value
        address = checkInt(dev.attrib, 'address')
        if None == address:
            perr("Attribute 'address' missing or has non-integer content in {}".format(ldl))
            continue
        # Modbus device address, 0=broadcast, 248..255=reserved
        if address < 1 or address > 255:
            perr("Address '{}' out of range [1..247] in {}".format(address, ldl))
            continue
        if address > 247:
            pwarn("Address '{}' is a Modbus reserved value. Valid range [1..247] in {}".format(address, ldl))

        # Don't allow duplicates in the list
        if name in devlist:
            perr("Device name '{}' already defined in {}".format(name, ldl))
            continue
        if address in addrlist:
            perr("Device '{}': address '{}' already used for device '{}' in {}".format(name, address, addrlist[address], ldl))
            continue

        addrlist[address] = name
        devlist[name] = address
        if verbose:
            print("Device bus ID 0x{0:02x} ({0:3}) ==> '{1}'".format(address, name))
    # end for dev in devs:

    # We should have one device on top of broadcast
    if len(devlist) < 2:
        pwarn("No devices defined other than broadcast")

    return devlist

#
# Parse optional attribute flags
#
def parseOptFlags(dev, attrs, cflags, pflags, ers):
    cflags |= MBCCB_CMDF_TIMESOUT if getBoolean(attrs, 'timesout') else 0
    cflags |= MBCCB_CMDF_BCANSWER if getBoolean(attrs, 'bcanswer') else 0
    cflags |= MBCCB_CMDF_NOANSWER if getBoolean(attrs, 'noanswer') else 0
    cflags |= MBCCB_CMDF_RESEND   if getBoolean(attrs, 'resend')   else 0
    # The default of writeflush depends on the global setting
    cflags |= MBCCB_CMDF_WFLUSH   if getBoolean(attrs, 'writeflush') else 0
    cflags &= ~MBCCB_CMDF_WFLUSH  if False == getBoolean(attrs, 'writeflush') else ~0
    # Scale and clamp can be default on. This allows them to be turned off.
    pflags |= MBCCB_PINF_SCALE  if getBoolean(attrs, 'scale')     else 0
    pflags |= MBCCB_PINF_CLAMP  if getBoolean(attrs, 'clamp')     else 0
    pflags &= ~MBCCB_PINF_SCALE if False == getBoolean(attrs, 'scale') else ~0
    pflags &= ~MBCCB_PINF_CLAMP if False == getBoolean(attrs, 'clamp') else ~0
    if 'broadcast' != dev and 0 != (cflags & MBCCB_CMDF_BCANSWER):
        perr("Cannot use 'bcanswer' attribute on non-broadcast target '{}' in {}".format(dev, ers));
    return cflags, pflags

#
# Calculate the timeout for a command based on the number of bytes
# sent/receives in worst case scenario, double it for processing and add
# state-machine overhead.
#
def calcTimeout(function, count, mtype, cfgp):
    # Find out the byte-size of the command+reply
    if function in [R_COILS, R_INPUTS]:
        n = 8 + 5 + ((count + 7) // 8)
    elif function in [W_COILS]:
        n = 9 + ((count + 7) // 8) + 8
    elif function in [R_REGISTERS, R_INPUTREGS]:
        n = 8 + 3 + count * MBTBYTESIZES[mbtOrder(mtype)]
    elif function in [W_REGISTER, W_COIL]:
        n = 8 + 8
    elif function in [W_REGISTERS]:
        n = 9 + count * MBTBYTESIZES[mbtOrder(mtype)] + 8
    else:
        sys.exit("Unknown function '{}' in calcTimeout, aborting...".format(function))
    bits = 1 + 8 + 1
    bits += 1 if cfgp['parity'] != 0 else 0
    bits += 1 if cfgp['stopbits'] == 1 else 0
    bits *= n   # Number of bits send+received
    # Worst case transmission is one character per just under 2.5 character
    # times (because of max. 1.5 inter-character gap allowance).
    bits *= 2.5
    # We allow the whole lot to take twice as long
    # Convert to microseconds based on baudrate
    # and add twice the state-machine base overhead based on a 1kHz servo-thread
    return int((2 * bits * 1000000.0 + cfgp['baudrate'] - 1) / cfgp['baudrate']) + 2*8000

#
# Extract the 'timeout' or 'timeoutbits' attribute and return the timeout value
# or the global default.
#
def getTimeout(attrib, ers):
    if 'timeout' in attrib:
        if "AUTO" == attrib['timeout'].upper():
            tov = 0
        else:
            # A specific timeout for this command
            tov = checkInt(attrib, 'timeout')
            if None == tov or tov < 1 or tov > MAXDELAY:
                perr("Attribute 'timeout' out of range [1..{}] in {}".format(MAXDELAY, ers))
                return None
        if 'timeoutbits' in attrib:
            pwarn("Attribute 'timeout' has preference over 'timeoutbits' in {}".format(ers))
    elif 'timeoutbits' in attrib:
        tob = checkInt(attrib, 'timeoutbits')
        if None == tob or tob < 0 or tob > MAXDELAYBITS:
            perr("Attribute 'timeoutbits' out of range [0..{}] in {}".format(MAXDELAYBITS, ers))
            return None
        tov = math.ceil(tob * 1000000.0 / configparams['baudrate'])
    else:
        tov = configparams['timeout']   # Use global timeout
    return tov

#
# Get the device name
#
def getDevice(attrib, ers):
    if 'device' not in attrib:
        perr("Attribute 'device' missing in {}".format(ers))
        return None
    # Device must be known
    dev = attrib['device']
    if dev not in devices:
        perr("Device name '{}' not found in devices list {}".format(dev, ers))
        return None
    return dev

#
# Get the target Modbus address
#
def getAddress(attrib, ers):
    addr = checkInt(attrib, 'address')
    if None == addr:
        perr("Attribute 'address' invalid or missing in {}".format(ers))
        return None
    if addr < 0 or addr > 65535:
        perr("Address '{}' out of range [0..65535] in {}".format(addr, ers))
        return None
    return addr

#
# Extract and check the 'function' attribute
#
def getFunction(attrib, ers):
    if 'function' not in attrib:
        perr("Attribute 'function' missing in {}".format(ers))
        return None
    name = attrib['function'].upper()
    if name in FUNCTIONS:
        return name # We're done if we recognize the name/value
    # Maybe it is a hex number
    try:
        val = int(name, 0)
    except (ValueError, TypeError) as err:
        perr("Invalid function '{}' in {}".format(attrib['function'], ers))
        return None
    if val not in FUNCNAMES:
        perr("Function '{}' not supported in {}".format(attrib['function'], ers))
    return FUNCNAMES[val]

#
# Check the allowed attributes for a tag
#
def checkAttribs(have, may, suffix):
    bad = list(set(have) - set(may))
    for a in bad:
        pwarn("Unrecognized attribute '{}' in {}".format(a, suffix))

#
#
#
def printCommsOverride(cfgs, cnt):
    if sum([cfgs[k] != configparams[k] for k in RATEATTRIB]) == 0:
        print("Init {:2}: Comms set to defaults".format(cnt))
        return

    print("Init {:2}: Comms override: baudrate={}, parity={}, stopbits={}, rxdelay={}, txdelay={}, drivedelay={}, icdelay={}"
            .format(cnt, cfgs['baudrate'], ['None','Odd','Even', ''][cfgs['parity']], cfgs['stopbits'],
                    getAutoFmt(cfgs['rxdelay'], ""), getAutoFmt(cfgs['txdelay'], ""),
                    getAutoFmt(cfgs['drivedelay'], ""), getAutoFmt(cfgs['icdelay'], "")))

#
# Parse the <initlist> tag content
#
def handleInits(inits):
    initlist = []
    cfgs = configparams # This is the default config
    for cmd in inits:
        lil = "initlist/command[{}]".format(1 + len(initlist))
        if cmd.tag != 'command':
            perr("Expected <command> tag as child of <initlist> in initlist/{}".format(cmd.tag))
            continue

        # This may be a delay command
        if 'delay' in cmd.attrib:
            # A delay between init commands
            checkAttribs(cmd.attrib, DELAYATTRIB, lil)
            delay = checkInt(cmd.attrib, 'delay')
            if None == delay or delay < 0 or delay > MAXDELAY:
                perr("Attribute 'delay' out of range [0..{}] in {}".format(MAXDELAY, lil))
                continue
            initlist.append({'delay': delay})
            if verbose:
                print("Init {:2}: delay {} microseconds".format(len(initlist), delay))
            continue

        # This may be a communication override command
        if len(set(cmd.attrib).intersection(RATEATTRIB)) > 0:
            # Set override communication parameters
            checkAttribs(cmd.attrib, RATEATTRIB, lil)
            cfgs = verifyConfigParams(CONFIGDEFAULT | cmd.attrib)    # Setup new config set
            if None == cfgs:
                perr("Invalid communication parameters in {}".format(lil))
                continue
            initlist.append(cfgs)   # Includes all required indices
            if verbose:
                printCommsOverride(cfgs, len(initlist))
            continue

        # An init command
        checkAttribs(cmd.attrib, INITATTRIB, lil)

        # Must have a device
        device = getDevice(cmd.attrib, lil)
        if None == device:
            continue

        # Get optional attrib flags
        cflags, pflags = parseOptFlags(device, cmd.attrib, 0, 0, lil)
        if 0 != cflags & ~MBCCB_CMDF_INITMASK:
            pwarn("Additional flags '0x{:04x}' (allowed=0x{:04x}) in {}".format(cflags, MBCCB_CMDF_INITMASK, lil))

        # Find timeout to use
        timeout = getTimeout(cmd.attrib,lil)
        if None == timeout:
            continue

        # Must have a function
        # Only known functions are allowed
        function = getFunction(cmd.attrib, lil)
        if None == function:
            continue

        maxcount = FUNCTIONS[function][1]
        function = FUNCTIONS[function][0]

        # Need a target address
        address = getAddress(cmd.attrib, lil)
        if None == address:
            continue

        # Data to be sent with the command
        datalist = []
        dlnbytes = 0;
        err = False
        for data in cmd:
            ldl = "{}/data[{}]".format(lil, (1 + len(datalist)))
            if data.tag == 'description':
                continue    # Ignored
            if data.tag != 'data':
                perr("Expected <data> tag instead of '{}' as child in {}".format(data.tag, lil))
                err = True
                break

            checkAttribs(data.attrib, DATAATTRIB, ldl)

            # Write function must have data
            if function not in WRITEFUNCTIONS:
                perr("Only write function can have <data> tag(s) in {}".format(data.tag, lil))
                err = True
                break
            # Get the type of the data
            mtype = data.attrib['modbustype'].upper() if 'modbustype' in data.attrib else 'U_AB'
            if mtype not in MBTYPES:
                perr("Invalid modbustype '{}' in {}".format(mtype, lil))
                err = True
                break
            mtype = MBTYPES[mtype][0]

            if MBT_F == mbtType(mtype):
                # Handle floating point target
                if function not in [W_REGISTER, W_REGISTERS]:
                    perr("Floating point data only possible for W_REGISTER(6)/W_REGISTERS(16) functions in {}".format(ldl))
                    err = True
                    break
                if W_REGISTER == function and mtype not in [F_AB, F_BA]:
                    perr("Floating point value requires 16-bit type for W_REGISTER(6) in {}".format(ldl))
                    err = True
                    break

                value = checkFlt(data.attrib, 'value')
                if None == value:
                    perr("Attribute 'value' invalid or missing in {}".format(ldl))
                    err = True
                    break
                if mtype in [F_AB, F_BA]:
                    # 16-bit float
                    if value < -65504.0 or value > 65504.0:
                        perr("Attribute 'value' out of range [-65504.0,+65504.0] for 16-bit float {}".format(ldl))
                        err = True
                        break
                    datalist.append(mangle16(struct.pack(">e", value), mtype))
                    dlnbytes += 2
                elif mtype in [F_ABCD, F_BADC, F_CDAB, F_DCBA]:
                    # 32-bit float
                    if value <= -3.4e38 or value >= +3.4e38:    # +/- 3.4028234664e38
                        perr("Attribute 'value' out of range [-3.4e38,+3.4e38] for 32-bit float {}".format(ldl))
                        err = True
                        break
                    datalist.append(mangle32(struct.pack(">f", value), mtype))
                    dlnbytes += 4
                else: # value is fine, otherwise we'd get an OverflowError on conversion
                    # 64-bit float
                    if value <= -1.7e308 or value >= +1.7e308:    # +/- 1.7976931348623157e308
                        perr("Attribute 'value' out of range [-1.7e308,+1.7e308] for 64-bit float {}".format(ldl))
                        err = True
                        break
                    # XXX: +/inf and NaN are also fine?
                    datalist.append(mangle64(struct.pack(">d", value), mtype))
                    dlnbytes += 8
            else: # no modbustype attribute or MBT_U or MBT_S
                value = checkInt(data.attrib, 'value')
                if None == value:
                    perr("Attribute 'value' invalid or missing in {}".format(ldl))
                    err = True
                    break
                # Write single coil usually has special value
                # We allow deviation because some devices allow it too
                if W_COIL == function:
                    if 'modbustype' in data.attrib and mtype != U_AB:
                        pwarn("Attribute 'modbustype' ignored for W_COIL(5) and always set to U_AB in {}".format(ldl))
                    if value != 0 and value != 0xff00:
                        pwarn("Data value '{}' for W_COIL(5) is normally expected to be 0 or 0xff00 (65280) in {}".format(value, ldl))
                    datalist.append(struct.pack('>H', value));
                    dlnbytes += 2
                elif W_COILS == function:
                    if 'modbustype' in data.attrib:
                        perr("Attribute 'modbustype' ignored for W_COILS(15) and always set to binary bits in {}".format(ldl))
                        err = True
                        break;
                    if value not in [0, 1]:
                        perr("Function W_COILS(15) only accepts binary data in range [0,1] in {}".format(ldl))
                        err = True
                        break;
                    datalist.append(struct.pack('>B', value));
                    dlnbytes += 1
                elif W_REGISTER == function:
                    if mtype not in [U_AB, U_BA, S_AB, S_BA]:
                        perr("Attribute 'modbustype' invalid for W_REGISTER(6), expected one of (U_AB,U_BA,S_AB,S_BA) in {}".format(ldl))
                        err = True
                        break;
                    if mtype in [U_AB, U_BA]:
                        # 16-bit unsigned
                        if value < 0 or value > 65535:
                            perr("Value '{}' outside valid range [0,65535] for W_REGISTER(6) in {}".format(value, ldl))
                            err = True
                            break;
                        datalist.append(mangle16(struct.pack('>H', value), mtype));
                        dlnbytes += 2
                    else:
                        # 16-bit signed
                        if value < -32768 or value > +32767:
                            perr("Value '{}' outside valid range [-32768,+32767] for W_REGISTER(6) in {}".format(value, ldl))
                            err = True
                            break;
                        datalist.append(mangle16(struct.pack('>h', value), mtype));
                        dlnbytes += 2
                else: # function == W_REGISTERS
                    if mtype in [U_AB, U_BA]:
                        # 16-bit unsigned
                        if value < 0 or value > 65535:
                            perr("Value '{}' outside valid range [0,65535] for W_REGISTER(6) in {}".format(value, ldl))
                            err = True
                            break;
                        datalist.append(mangle16(struct.pack('>H', value), mtype));
                        dlnbytes += 2
                    elif mtype in [S_AB, S_BA]:
                        # 16-bit signed
                        if value < -32768 or value > +32767:
                            perr("Value '{}' outside valid range [-32768,+32767] for W_REGISTER(16) in {}".format(value, ldl))
                            err = True
                            break;
                        datalist.append(mangle16(struct.pack('>h', value), mtype));
                        dlnbytes += 2
                    elif mtype in [U_ABCD, U_BADC, U_CDAB, U_DCBA]:
                        # 32-bit unsigned
                        if value < 0 or value > 0xffffffff:
                            perr("Value '{}' outside valid range [0,4294967295] for W_REGISTERS(16) in {}".format(value, ldl))
                            err = True
                            break;
                        datalist.append(mangle32(struct.pack(">I", value), mtype))
                        dlnbytes += 4
                    elif mtype in [S_ABCD, S_BADC, S_CDAB, S_DCBA]:
                        # 32-bit signed
                        if value < -2147483648 or value > +2147483647:
                            perr("Value '{}' outside valid range [-2147483648,+2147483647] for W_REGISTERS(16) in {}".format(value, ldl))
                            err = True
                            break;
                        datalist.append(mangle32(struct.pack(">i", value), mtype))
                        dlnbytes += 4
                    elif MBT_S == mbtType(mtype):
                        # 64-bit signed
                        if value < 0x8000000000000000 or value > 0x7fffffffffffffff:
                            perr("Value '{}' outside valid 64-bit range [{},{}] for W_REGISTERS(16) in {}"
                                    .format(value, 0x8000000000000000, 0x7fffffffffffffff, ldl))
                            err = True
                            break;
                        datalist.append(mangle64(struct.pack(">q", value), mtype))
                        dlnbytes += 8
                    else: # MBT_U == mbtType(mtype)
                        # 64-bit unsigned
                        if value > 0xffffffffffffffff:
                            perr("Value '{}' outside valid 64-bit range [0,{}] for W_REGISTERS(16) in {}"
                                    .format(value, 0xffffffffffffffff, ldl))
                            err = True
                            break;
                        datalist.append(mangle64(struct.pack(">Q", value), mtype))
                        dlnbytes += 8
                # end if [functions]
            # end if MBT_x types
        # end for data in cmd:
        if err:
            continue

        # Read functions must have a count
        if function not in WRITEFUNCTIONS:
            if function in [R_COILS, R_REGISTERS, R_INPUTREGS]:
                count = checkInt(cmd.attrib, 'count')
                if None == count:
                    print("Attribute 'count' is missing or invalid in {}".format(lil))
                    continue
            else:
                count = 1
        else:
            count = dlnbytes // 2 if function != R_COILS else dlnbytes

        # We cannot write beyond the address region in multi-writes
        if address + maxcount > 65536:
            maxcount = 65536 - address

        if W_COILS != function and 0 != dlnbytes & 1:
            perr("Odd number of data bytes for function {}({}) in {}".format(FUNCNAMES[function], function, lil))
            continue

        if count < 1 or count > maxcount:
            if count + address > 65535:
                perr("Count would wrap address range in {}".format(lil))
            perr("Data element count '{}' is out of range [1..{}] for function {}({}) in {}"
                 .format(count, maxcount, FUNCNAMES[function], function, lil))
            continue

        if 0 == timeout:
            timeout = calcTimeout(function, count, MBT_AB, cfgs);

        initlist.append({'device': device, 'mbid': devices[device], 'function': function, 'count': count,
                         'address': address, 'data': datalist, 'timeout': timeout, 'flags': cflags })
        if verbose:
            print("Init {:2}: {} {}({}) addr=0x{:04x} flags={} timeout={} data="
                  .format(len(initlist), device, FUNCNAMES[function], function, address, cflagList(cflags), timeout), end='')
            fmt = "{:01x}" if W_COILS == function else "{:02x}"
            print(','.join((''.join(fmt.format(c) for c in d)) for d in datalist))

    # end for cmd in init:

    if sum([cfgs[k] != configparams[k] for k in RATEATTRIB]) > 0:
        pwarn("Communication override(s) do not return to defaults. Adding return to default values or the <commands> may fail.")
        initlist.append(configparams)   # Includes all defaults required
        if verbose:
            printCommsOverride(configparams, len(initlist))
    return initlist

#
# Return the 'modbustype' attribute translated
#
def getModbusType(tag, ers):
    if 'modbustype' not in tag.attrib:
        return None
    mbt = tag.attrib['modbustype'].upper()
    if mbt not in MBTYPES:
        perr("Invalid modbustype '{}' in {}".format(tag.attrib['modbustype'], ers))
        return None
    return MBTYPES[mbt];

#
# Return the 'haltype' attribute translated
#
def getHalType(tag, ers):
    if 'haltype' not in tag.attrib:
        return None
    ht = tag.attrib['haltype'].upper()
    if ht not in HALTYPES:
        perr("Invalid haltype '{}' in {}".format(tag.attrib['haltype'], ers))
        return None
    return HALTYPES[ht];

#
# Parse the <commands> tag content
#
def handleCommands(commands):
    cmdlist = []
    pinlistall = []
    for cmd in commands:
        lcl = "commands/command[{}]".format(1 + len(cmdlist))
        if cmd.tag != 'command':
            perr("Expected <command> tag as child of <commands> in commands/{}".format(cmd.tag))
            continue

        if 'delay' in cmd.attrib:
            # A delay between commands
            checkAttribs(cmd.attrib, DELAYATTRIB, lcl)
            delay = checkInt(cmd.attrib, 'delay')
            if None == delay or delay < 0 or delay > MAXDELAY:
                perr("Attribute 'delay' out of range [0..{}] {}".format(MAXDELAY, lcl))
                continue
            cmdlist.append({'delay': delay})
            if verbose:
                print("Command {:2}: delay {} microseconds".format(len(cmdlist), delay))
            continue

        # Handling a normal command
        checkAttribs(cmd.attrib, CMDSATTRIB, lcl)

        # Device must be known
        device = getDevice(cmd.attrib, lcl)
        if None == device:
            continue

        # Find timeout to use
        timeout = getTimeout(cmd.attrib,lcl)
        if None == timeout:
            continue

        if 'interval' in cmd.attrib:
            if 'ONCE' == cmd.attrib['interval'].upper():
                interval = 0xffffffff   # Marks infinity
            else:
                interval = checkInt(cmd.attrib, 'interval')
                if None == interval or interval < 0 or interval > MAXINTERVAL:
                    perr("Attribute 'interval' out of range [once,0..{}] {}".format(MAXINTERVAL, lcl))
                    continue
        else:
            interval = configparams['interval'] # Global default

        # Must have a target address
        address = getAddress(cmd.attrib, lcl)
        if None == address:
            continue

        # The command function to perform
        function = getFunction(cmd.attrib, lcl)
        if None == function:
            continue
        maxcount = FUNCTIONS[function][1]
        function = FUNCTIONS[function][0]

        # check depends on unicast or broadcast
        if 'broadcast' == device and function not in WRITEFUNCTIONS:
            perr("Function '{}' not available for broadcast in {}".format(FUNCNAMES[function], lcl))
            continue

        # How many pins to read/write
        if  function in [W_REGISTER, W_COIL]:
            count = 1
            if 'count' in cmd.attrib:
                pwarn("Attribute 'count' ignored for {}({}) in {}".format(FUNCNAMES[function], function, lcl))
        else:
            count = checkInt(cmd.attrib, 'count')
            if None == count:
                count = 0

        if function in BITFUNCTIONS:
            # Coils and inputs are binary and always map to HAL_BIT
            if 'modbustype' in cmd.attrib:
                pwarn("Attribute 'modbustype' ignored for bit functions in {}".format(lcl))
            if 'haltype' in cmd.attrib:
                pwarn("Attribute 'haltype' ignored for bit functions (always HAL_BIT) in {}".format(lcl))
            defmtype = [-1, 2000, 1] # fake mtype
            defhtype = HAL_BIT
        else:
            # These are defaults for R_REGISTERS, R_INPUTREGS, W_REGISTER and W_REGISTERS
            defmtype = getModbusType(cmd, lcl)
            defhtype = getHalType(cmd, lcl)
            if None != defmtype and W_REGISTER == function:
                if mbtOrder(defmtype[0]) not in [MBT_AB, MBT_BA]:
                    perr("Function W_REGISTER(6) requires a 16-bit modbustype (S_AB,S_BA,U_AB,U_BA,F_AB,F_BA) in {}".format(lcl))
                    continue
                assert 1 == count # This should have been set above

        # Get optional attrib flags
        # scale is default for float pins
        # clamp is default on all pins
        # writeflush depends on global setting
        defcflag  = MBCCB_CMDF_WFLUSH if configparams['writeflush'] else 0
        defpflag  = MBCCB_PINF_SCALE if defhtype == HAL_FLT else 0
        defpflag |= MBCCB_PINF_CLAMP if function in REGFUNCTIONS else 0
        cflags, pflags = parseOptFlags(device, cmd.attrib, defcflag, defpflag, lcl)
        if 0 != cflags & ~MBCCB_CMDF_MASK:
            pwarn("Additional cmd flags '0x{:04x}' (allowed=0x{:04x}) in {}".format(cflags, MBCCB_CMDF_MASK, lcl))
        if 0 != pflags & ~MBCCB_PINF_MASK:
            pwarn("Additional pin flags '0x{:04x}' (allowed=0x{:04x}) in {}".format(pflags, MBCCB_PINF_MASK, lcl))

        if (pflags & MBCCB_PINF_SCALE) and defhtype in [HAL_U32, HAL_U64]:
            pwarn("Unsigned hal types cannot be scaled, disabling in {}".format(lcl))
            pflags &= ~MBCCB_PINF_SCALE & 0xffff

        if None != defmtype and ((0 == (pflags & MBCCB_PINF_SCALE)) and
                   ((MBT_S == mbtType(defmtype[0]) and defhtype in [HAL_U32, HAL_U64])
                 or (MBT_U == mbtType(defmtype[0]) and defhtype in [HAL_S32, HAL_S64]))):
            pwarn("Signedness mismatch between haltype={} and modbustype={} may give wrong results in {}"
                    .format(HALNAMES[defhtype], MBNAMES[defmtype[0]], lcl))

        if None != defmtype and mbtOrder(defmtype[0]) >= MBT_ABCDEFGH and defhtype in [HAL_U32, HAL_S32]:
            pwarn("Haltype destination '{}' is smaller than Modbus source type {} in {}"
                    .format(HALNAMES[defhtype], MBNAMES[defmtype[0]], lcl))

        if (cflags & MBCCB_CMDF_RESEND) and interval == 0xffffffff:
            perr("Interval 'once' and 'resend' are mutually exclusive in {}".format(lcl))
            continue

        # Write flush only makes sense in write functions
        if function not in WRITEFUNCTIONS:
            cflags &= ~MBCCB_CMDF_WFLUSH

        cmdname = cmd.attrib['name'] if 'name' in cmd.attrib else None
        if None != cmdname:
            if not pinpattern.match(cmdname):
                perr("Attribute 'name' contains invalid characters {}".format(lcl))
                continue
            if len(cmdname) > MAXPINNAME - 3:
                perr("Attribute 'name' longer than {} characters in {}".format(MAXPINNAME - 3, lcl))
                continue

        # Iterate all pin tags
        # pinlist contains set of [pintag, mtype, htype, flags, regoffset]
        pinlist = []
        err = False
        regofs = 0
        for pin in cmd:
            lpl = "{}/pin[{}]".format(lcl, (1 + len(pinlist)))
            if pin.tag == 'description':
                continue
            if pin.tag != 'pin':
                perr("Expected <pin> tag {}".format(lcl))
                err = True
                break

            # We may skip registers in register reads
            if 'skip' in pin.attrib:
                if function not in [R_REGISTERS, R_INPUTREGS, W_REGISTERS]:
                    perr("Attribute 'skip' Not allowed with function {}({}) in {}"
                            .format(FUNCNAMES[function], function, lpl))
                    err = True
                    break
                checkAttribs(pin.attrib, SKIPATTRIB, lpl)
                skip = checkInt(pin.attrib, 'skip')
                if None == skip or skip < 1 or skip > 24:
                    perr("Attribute 'skip' in invalid or out of range [1,24] in {}".format(lpl))
                    err = True
                    break
                elif skip > 11:
                    pwarn("Attribute 'skip' larger than 11 registers. May be better to split command in {}".format(lpl))
                regofs += skip
                continue

            # Normal pin attribs
            checkAttribs(pin.attrib, PINSATTRIB, lpl)

            # Setup pin name
            if 'name' not in pin.attrib:
                if None == cmdname:
                    perr("Attribute 'name' missing in {}".format(lpl))
                    err = True
                    break
                pinname = "{}-{:02d}".format(cmdname, len(pinlist))
            else:
                pinname = pin.attrib['name']
                if not pinpattern.match(pinname):
                    perr("Attribute 'name' contains invalid characters in {}".format(lpl))
                    err = True
                    break
            if len(pinname) > MAXPINNAME:
                perr("Pin name '{}' longer than {} characters in {}".format(pinname, MAXPINNAME, lpl))
                err = True
                break
            pintag = "{}.{}".format(device, pinname)
            if pintag in pinlistall:
                perr("Pin name '{}' already in use in {}".format(pintag, lpl))
                err = True
                break

            if function in BITFUNCTIONS:
                if 'modbustype' in pin.attrib:
                    pwarn("Attribute 'modbustype' ignored for bit functions in {}".format(lpl))
                if 'haltype' in pin.attrib:
                    pwarn("Attribute 'haltype' ignored for bit functions in {}".format(lpl))
                pinlist.append({'pin': pintag, 'mtype': 0, 'htype': HAL_BIT, 'flags': 0, 'regofs': regofs})
                pinlistall.append(pintag)
                regofs += 1
                continue

            # Register functions can have override attributes
            pmtype = getModbusType(pin, lpl)
            phtype = getHalType(pin, lpl)
            if None == pmtype and None == defmtype:
                perr("No 'modubustype' attribute defined in command or pin tag in {}".format(lpl))
                err = True
                break
            if None == phtype and None == defhtype:
                perr("No 'haltype' attribute defined in command or pin tag in {}".format(lpl))
                err = True
                break
            if None == pmtype:
                pmtype = defmtype
            if None == phtype:
                phtype = defhtype
            assert pmtype != None   # Must have hal and modbus types
            assert phtype != None
            # W_REGISTER(S), R_REGISTERS and R_INPUTREGS can have special
            if HAL_BIT == phtype and MBT_U != mbtType(pmtype):
                perr("Haltype HAL_BIT with register function must use unsigned modbustype in {}".format(lpl))
                err = True
                break
            # scale and clamp flags
            cf, pf = parseOptFlags(device, pin.attrib, 0, pflags, lpl)
            if (pf & MBCCB_PINF_SCALE) and phtype in [HAL_U32, HAL_U64]:
                pwarn("Unsigned hal types cannot be scaled, disabling in {}".format(lpl))
                pf &= ~MBCCB_PINF_SCALE
            msz = MBTBYTESIZES[mbtOrder(pmtype[0])]
            hsz = 4 if phtype in [HAL_U32, HAL_S32] else 8
            if ((0 == (pf & MBCCB_PINF_SCALE)) and msz >= hsz and
                       ((MBT_S == mbtType(pmtype[0]) and phtype in [HAL_U32, HAL_U64])
                     or (MBT_U == mbtType(pmtype[0]) and phtype in [HAL_S32, HAL_S64]))):
                pwarn("Signedness mismatch between haltype={} and modbustype={} may give wrong results in {}"
                        .format(HALNAMES[phtype], MBNAMES[pmtype[0]], lpl))
            if (mbtOrder(pmtype[0])) >= MBT_ABCDEFGH and phtype in [HAL_U32, HAL_S32]:
                pwarn("Haltype destination '{}' is smaller than Modbus source type {} in {}"
                        .format(HALNAMES[phtype], MBNAMES[pmtype[0]], lpl))

            if ((mbtOrder(pmtype[0]) >= MBT_ABCD and 0 != (regofs & 1))
                or (mbtOrder(pmtype[0]) >= MBT_ABCDEFGH and 0 != (regofs & 3))):
                pwarn("Multi-register type '{}' not aligned to natural boundary in {}".format(MBNAMES[pmtype[0]], lpl))

            pinlist.append({'pin': pintag, 'mtype': pmtype[0], 'htype': phtype, 'flags': pf, 'regofs': regofs})
            pinlistall.append(pintag)
            regofs += MBTBYTESIZES[mbtOrder(pmtype[0])] // 2
        # endfor pin in cmd
        if err:
            continue

        if len(pinlist) <= 0:
            # If no <pin> list, then we require a count
            if 0 == count:
                perr("Attribute 'count' invalid or missing in {}".format(lcl))
                continue
        else:
            # We have a <pin> list; it may need to be appended
            if 0 == count:
                count = len(pinlist)    # pin list determines count
            elif len(pinlist) > count:
                perr("Number of pins '{}' larger than (max)count '{}' in {}".format(len(pinlist), count, lcl))
                continue

        # Append default naming scheme "name-XX"
        # But we must have default name and types defined to do so
        if len(pinlist) < count:
            if None == defmtype:
                perr("Attribute 'modbustype' missing in {}".format(lcl))
                continue
            if None == defhtype:
                perr("Attribute 'haltype' missing in {}".format(lcl))
                continue
            if None == cmdname:
                perr("Attribute 'name' missing in {}".format(lcl))
                continue

        # Fill pin list with generated names
        while len(pinlist) < count:
            pintag = "{}.{}-{:02d}".format(device, cmdname, len(pinlist))
            if pintag in pinlistall:
                perr("Pin name '{}' already in use in {}".format(pintag, lcl))
                err = True
                break
            if function in BITFUNCTIONS:
                pinlist.append({'pin': pintag, 'mtype': 0, 'htype': HAL_BIT, 'flags': 0, 'regofs': regofs})
                regofs += 1
            else:
                pinlist.append({'pin': pintag, 'mtype': defmtype[0], 'htype': defhtype, 'flags': pflags, 'regofs': regofs})
                regofs += MBTBYTESIZES[mbtOrder(defmtype[0])] // 2
            pinlistall.append(pintag)
        if err:
            continue

        # pinlist has all pins
        # regofs has the total number of registers to read/write

        # Must be in bounds [1..maxcount]
        if count < 1 or count > maxcount or regofs > maxcount:
            perr("Number of pins or registers '{}' out of range [1..{}] in {}".format(count, maxcount, lcl))
            continue

        # Don't wrap address
        if regofs + address > 65535:
            perr("Reading {} registers from address {} wraps the address counter in {}".format(regofs, address, lcl))
            continue

        if 0 == timeout:
            timeout = calcTimeout(function, regofs, MBT_AB, configparams);

        cmdlist.append({'device': device, 'mbid': devices[device], 'function': function,
                        'timeout': timeout, 'address': address, 'count': count,
                        'regcnt': regofs, 'pins': pinlist, 'flags': cflags,
                        'interval': interval })
        if verbose:
            print("Command {:2}: {} {}({}) addr=0x{:04x} flags={} interval={} timeout={}"
                .format(len(cmdlist), device, FUNCNAMES[function], function, address, cflagList(cflags),
                        interval, timeout))
            io = "in " if function in WRITEFUNCTIONS else "out"
            for p in range(len(pinlist)):
                pin = pinlist[p]
                if HAL_BIT == pin['htype']:
                    mbn = 'HAL_BIT<=>BIT'
                else:
                    mbn = '{}<=>{}'.format(HALNAMES[pin['htype']], MBNAMES[pin['mtype']])
                print("  pin {:2} ({}): {:24} {} flags={} addr=0x{:04x}".format(p+1, io, pin['pin'],
                        mbn, pflagList(pin['flags']), address + pin['regofs']))
                if pin['flags'] & MBCCB_PINF_SCALE:
                    if function in WRITEFUNCTIONS:
                        pt = HALNAMES[pin['htype']]
                    else:
                        pt = {MBT_U: "HAL_U64", MBT_S: "HAL_S64", MBT_F: "HAL_FLOAT"}[mbtType(pin['mtype'])]
                    print("         (in ): {:24} {}".format(pin['pin']+".offset", pt))
                    print("         (in ): {:24} HAL_FLOAT".format(pin['pin']+".scale"))
                    if function not in WRITEFUNCTIONS:
                        print("         (out): {:24} HAL_FLOAT".format(pin['pin']+".scaled"))
    # end for cmd in commands:

    return cmdlist

#
# Main program
#
def main():
    # Get the command-line options
    try:
        opts, args = getopt.getopt(sys.argv[1:], "ho:v", ["help", "output=", "verbose"])
    except getopt.GetoptError as err:
        print(err, file=sys.stderr)  # Something like "option -a not recognized"
        return 1

    # Check the options
    output = "mesamodbus.output.mbccb"
    global verbose
    for o, a in opts:
        if o in ("-v", "--verbose"):
            verbose = True
        elif o in ("-o", "--output"):
            output = a
        elif o in ("-h", "--help"):
            usage()
        else:
            print("Unhandled option: '{}'".format(o), file=sys.syderr);
            return 1

    if len(args) < 1:
        print("Must have one file argument.", file=sys.stderr);
        return 1
    if len(args) > 1:
        print("Cannot do more than one file at a time.", file=sys.stderr);
        return 1

    global inputfilename
    inputfilename = args[0]

    # Parse the XML source
    try:
        mbccs = ET.parse(inputfilename);
    except ET.ParseError as err:
        perr("line {}, char {}: {}".format(*err.position, XP.ErrorString(err.code)))
        return 1

    root = mbccs.getroot();
    if root.tag != "mesamodbus":
        perr("Expected <mesamodbus> root tag")
        return 1

    checkAttribs(root.attrib, MESAATTRIB, "<mesamodbus>")

    # Merge and check the comms attributes
    global configparams
    configparams = verifyConfigParams(configparams | root.attrib)
    if None == configparams:
        return 1

    if verbose:
        print("Default communication parameters and setup:")
        print("  baudrate  : {}".format(configparams['baudrate']))
        print("  parity    : {}".format(['None', 'Odd', 'Even'][configparams['parity']]))
        print("  stopbits  : {}".format(configparams['stopbits']))
        print("  icdelay   : {}".format(getAutoFmt(configparams['icdelay'], "bits")))
        print("  rxdelay   : {}".format(getAutoFmt(configparams['rxdelay'], "bits")))
        print("  txdelay   : {}".format(getAutoFmt(configparams['txdelay'], "bits")))
        print("  drivedelay: {}".format(getAutoFmt(configparams['drivedelay'], "bits")))
        print("  timeout   : {}".format(getAutoFmt(configparams['timeout'], "microseconds")))
        print("  suspend   : {}".format("true" if configparams['suspend'] else "false"))

    # Parse the nodes
    global devices
    global errorflag
    initlist = None    # Modbus devices init commands
    commands = None    # Modbus I/O commands
    for node in root:
        if node.tag == "devices":
            if None != devices:
                perr("Multiple <devices> tags")
                continue
            devices = handleDevices(node)
            if None == devices:
                return 1
        else:
            if None == devices:
                perr("The <devices> tag must be declared first")
                break   # This error is fatal, we need the devices list

            if node.tag == 'initlist':
                if None != initlist:
                    perr("Multiple <initlist> tags")
                    continue
                initlist = handleInits(node)
                if None == initlist:
                    errorflag = True
                    continue
            elif node.tag == 'commands':
                if None != commands:
                    perr("Multiple <commands> tags")
                    continue
                commands = handleCommands(node)
                if None == commands:
                    errorflag = True
                    continue
            else:
                perr("Invalid/unknown tag '{}'".format(node.tag))

    # Quit if a parse error occurred
    if errorflag:
        return 1

    # Now construct the binary image
    npins = 0   # statistics (all mirrors and lies)
    ilb = []    # initlist binary hm2_modbus_mbccb_cmd_t
    cmb = []    # commands binary hm2_modbus_mbccb_cmd_t
    dlb = []    # datalist binary fragments
    dlblen = 0  # datalist binary accumulated length

    # Add one empty data fragment so that the data segment always has content.
    # This will make a zero dataptr or typeptr impossible if data is to be
    # attached to them.
    dlb.append(struct.pack(">B", 0))
    dlblen += 1

    #
    # The init commands are pre-packed, including mbid and function code. They
    # only need the CRC attached, which is done in the hm2_modbus module.
    # The command packets have the same structure layout but different fields
    # are used.
    #
    # typedef struct {
    #   rtapi_u8  mbid;  // Modbus device ID
    #   rtapi_u8  func;  // Function code, 0 for init
    #   rtapi_u16 flags; // Mostly quirks to handle, see MBCCB_CMDF_* defines
    #   union {
    #     struct {    // Command fields
    #       rtapi_u16 addr;     // Address
    #       rtapi_u16 pincnt;   // Number of pins
    #       rtapi_u16 regcnt;   // Number of registers
    #       rtapi_u16 unusedp1; // cmds 0 (drvdly)
    #       rtapi_u32 unusedp2; // cmds 0 (icdelay)
    #       rtapi_u32 typeptr;  // Type and address offset list
    #       rtapi_u32 interval; // The interval to repeat this command
    #       rtapi_u32 timeout;  // Response timeout or delay in microseconds
    #     };
    #     struct {    // Init comm change fields
    #       rtapi_u16 metacmd;  // Meta command
    #       rtapi_u16 rxdelay;  // init comm change
    #       rtapi_u16 txdelay;  // init comm change
    #       rtapi_u16 drvdelay; // init comm change
    #       rtapi_u32 icdelay;  // init comm change (unusedp1)
    #       rtapi_u32 unusedi1; // init 0 (typeptr)
    #       rtapi_u32 unusedi2; // init 0 (interval)
    #       rtapi_u32 baudrate; // init comm change
    #     };
    #   };
    #   rtapi_u32 dataptr; // Pin names, packet data for init
    # } hm2_modbus_mbccb_cmds_t;
    #
    for i in initlist:
        if 'delay' in i:        # meta-command 0
            ilb.append(struct.pack(">BBHHHHHIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, i['delay'], 0))
        elif 'baudrate' in i:   # meta-command 1
            flg  = MBCCB_CMDF_PARITYEN  if i['parity'] != 0 else 0
            flg |= MBCCB_CMDF_PARITYODD if i['parity'] == 1 else 0
            flg |= MBCCB_CMDF_STOPBITS2 if i['stopbits'] == 2 else 0
            ilb.append(struct.pack(">BBHHHHHIIIII", 0, 0, flg, 1, i['rxdelay'], i['txdelay'],
                                                    i['drivedelay'], i['icdelay'], 0, 0, i['baudrate'], 0))
        else:
            mbid = i['mbid']
            func = i['function']
            addr = i['address']
            ilb.append(struct.pack(">BBHHHHHIIIII", mbid, func, i['flags'], addr, 0, 0, 0, 0, 0, 0, i['timeout'], dlblen))
            # Precompile the data packet so we only need to copy it
            if func in [W_COIL, W_REGISTER]:  # Write single coil or single register
                # mbid, func, length address and value == 6 bytes
                assert len(i['data']) == 1
                assert len(i['data'][0]) == 2
                dlb.append(struct.pack(">BBBH", 6, mbid, func, addr))
                dlb.append(i['data'][0])
                dlblen += 6 + 1
            elif W_COILS == func:    # Write multiple coils
                nbytes = (len(i['data']) + 7) // 8
                # mbid, func, length address, count and bytecount == 6 + 1 bytes
                d = struct.pack(">BBBHHB", 6 + 1 + nbytes, mbid, func, addr, len(i['data']), nbytes)
                dlblen += 6 + 1 + nbytes + 1
                # Construct a bit string
                val = ''.join(format(bit[0], '01b') for bit in i['data'])
                # Split string in 8-bit sequences
                parts = [val[x:x+8] for x in range(0, len(val), 8)]
                assert nbytes == len(parts)
                # for all bytes, reverse bit string and convert to integer base 2
                for part in parts:
                    d += struct.pack("B", int(part[::-1], 2))
                dlb.append(d)
            elif W_REGISTERS == func:   # Write multiple registers
                d = b''                 # Construct the binary data string in 'd'
                for w in i['data']:     # Each part (already in correct byte-order)
                    d += w              # Concatenate parts
                ld = len(d)             # Resulting in 'ld' bytes in packet
                # mbid, func, length address, count and bytecount == 6 + 1 bytes
                d = struct.pack(">BBBHHB", 6 + 1 + ld, mbid, func, addr, ld // 2, ld) + d
                dlblen += 6 + 1 + 1 + ld
                dlb.append(d)
            elif func in [R_COILS, R_INPUTS, R_INPUTREGS, R_REGISTERS]:
                # mbid, func, address and number == 6 bytes
                dlb.append(struct.pack(">BBBHH", 6, mbid, func, addr, i['count']))
                dlblen += 6 + 1
            else:
                perr("Unhandled init function mbid={}, func={}, addr={}".format(mbid, func, addr))
    #
    # See structure layout above
    #
    for i in commands:
        if 'delay' in i:
            cmb.append(struct.pack(">BBHHHHHIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, i['delay'], 0))
        else:
            pinptr = dlblen
            typs = b''
            # Add pin names to data list
            for p in i['pins']:
                asc = p['pin'].encode('ascii')
                d = struct.pack(">B", len(asc)+1) + asc + struct.pack(">B", 0)
                dlblen += 1 + len(asc) + 1
                dlb.append(d)

                if i['function'] in REGFUNCTIONS:
                    # Add mtype, htype, flags, regoffset
                    typs += struct.pack(">BBBB", p['mtype'], p['htype'], p['flags'], p['regofs'])
            npins += len(i['pins'])

            if len(typs) > 0:
                # Align on % 4
                # Note that the /content/ must be aligned
                algn = (dlblen + 1) % 4
                if 3 == algn:
                    dlb.append(struct.pack(">B", 0))
                    dlblen += 1
                elif 2 == algn:
                    dlb.append(struct.pack(">BB", 1, 0))
                    dlblen += 2
                elif 1 == algn:
                    dlb.append(struct.pack(">BBB", 2, 0, 0))
                    dlblen += 3
                typeptr = dlblen
                dlblen += 1 + len(typs)
                dlb.append(struct.pack(">B", len(typs)) + typs)
            else:
                typeptr = 0

            cmb.append(struct.pack(">BBHHHHHIIIII", i['mbid'], i['function'], i['flags'], i['address'],
                                                     i['count'], i['regcnt'], 0, 0,
                                                     typeptr, i['interval'], i['timeout'], pinptr))

    # typedef struct {
    #   rtapi_u8  sig[8];   // Signature and version {'M','e','s','a','M','B','0','1'}
    #   rtapi_u32 baudrate;
    #   rtapi_u16 format;   // Parity and stopbits
    #   rtapi_u16 txdelay;  // Tx t3.5
    #   rtapi_u16 rxdelay;  // Rx t3.5
    #   rtapi_u16 drvdelay; // Delay from output enable to tx start
    #   rtapi_u16 icdelay;  // Rx inter-character timeout (t1.5)
    #   rtapi_u16 unused1;
    #   rtapi_u32 unused2[7];
    #   rtapi_u32 initlen;  // Length of init section
    #   rtapi_u32 cmdslen;  // Length of command section
    #   rtapi_u32 datalen;  // Length of data table
    # } hm2_modbus_mbccb_header_t;

    par  = [0, MBCCB_FORMAT_PARITYEN | MBCCB_FORMAT_PARITYODD, MBCCB_FORMAT_PARITYEN, 0]
    flg  = MBCCB_FORMAT_STOPBITS2 if configparams['stopbits'] == 2 else 0
    flg |= MBCCB_FORMAT_DUPLEX if configparams['duplex'] else 0
    flg |= MBCCB_FORMAT_SUSPEND if configparams['suspend'] else 0
    header = (struct.pack(">8sIHHHHHHIIIIIIIIII",
                        b'MesaMB01',
                        configparams['baudrate'],
                        par[configparams['parity']] | flg,
                        configparams['txdelay'],
                        configparams['rxdelay'],
                        configparams['drivedelay'],
                        configparams['icdelay'],
                        0, 0, 0, 0, 0, 0, 0, 0,
                        32*len(ilb), 32*len(cmb), dlblen))

    # Generate output
    fsize = len(header)
    try:
        with open(output, "wb") as f:
            f.write(header)
            for b in ilb:
                f.write(b)
                fsize += len(b)
            for b in cmb:
                f.write(b)
                fsize += len(b)
            for b in dlb:
                f.write(b)
                fsize += len(b)
    except OSError as err:
        print("Cannot open '{}' for output: {}".format(output, err.strerror))
        return 1

    if verbose:
        print("Wrote output file '{}':".format(output))
        print("  {:3} inits".format(len(ilb)))
        print("  {:3} commands".format(len(cmb)))
        print("  {:3} pins".format(npins))
        print("  {:3} data fragments".format(len(dlb) - npins))
        print("  total {} bytes".format(fsize))

    # and...done
    return 0

if __name__ == "__main__":
    sys.exit(main())
