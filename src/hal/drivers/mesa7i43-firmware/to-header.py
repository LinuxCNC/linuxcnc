#!/usr/bin/python


import sys, getopt


def bit_file_verify_and_print_string(title, character, data): 
    if data[0] != character:
        raise SystemExit, "doesnt look like a BIT file"

    string_len = (ord(data[1]) * 256) + ord(data[2])
    if string_len > 255:
        string_len = 255  # wtf?
    string_start = 3  # skip over header character and the string length
    string = data[string_start:(string_start+string_len-1)]
    sys.stderr.write(title + ": " + string + "\n")
    return string_len + 3


def bit_file_verify_and_print_header(data):
    if (data[0] != '\x00') or (data[1] != '\x09') or (data[11] != '\x00') or (data[12] != '\x01'):
        raise SystemExit, "doesn't look like a BIT file"

    index = 13

    length = bit_file_verify_and_print_string('Design name', 'a', data[index:])
    index += length

    length = bit_file_verify_and_print_string('Part name', 'b', data[index:])
    index += length

    length = bit_file_verify_and_print_string('Design date', 'c', data[index:])
    index += length

    length = bit_file_verify_and_print_string('Design time', 'd', data[index:])
    index += length

    if (data[index] != 'e'):
        raise SystemExit, "doesnt look like a BIT file"
    index += 1
    config_length = (ord(data[index]) * 2^24) + (ord(data[index+1]) * 2^16) + (ord(data[index+2]) * 2^8) + ord(data[index+3])
    sys.stderr.write("Config length: " + str(config_length) + "\n")
    index += 4

    return index


def reverse_bits(byte):
    reverse_table = (
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
    )
    return reverse_table[ord(byte)]


def filename_to_c_variable(filename):
    filename = filename.lower()
    try:
        filename = filename[filename.rindex("/"):]
    except ValueError:
        pass

    if filename[0].isalpha():
        variable_name = filename[0]
    elif filename[0].isdigit():
        variable_name = '_' + filename
    else:
        variable_name = ''

    for i, byte in enumerate(filename[1:]):
        if byte.isalnum():
            variable_name = variable_name + byte
        else:
            variable_name = variable_name + '_'

    return variable_name




if len(sys.argv) < 2:
    raise SystemExit, "no .BIT file supplied on command-line"

GPL=0
LGPL=0
EXTRA=[]
opts, args = getopt.getopt(sys.argv[1:], "gldc:")
for k, v in opts:
    if k == "-g":
	GPL = 1
    elif k == "-l":
	LGPL = 1
    elif k == "-d":
	DUAL_GPL_BSD = 1
    elif k == "-c":
	EXTRA.append("//    " + v)
    else:
	raise SystemExit, "Unknown argument: %r" % k

if len(EXTRA) > 0:
    sys.stdout.write("\n");
    sys.stdout.write("//    \n");
    sys.stdout.write("\n".join(EXTRA))
    sys.stdout.write("\n");
    sys.stdout.write("//    \n");

print """
//    This is a generated file; the "corresponding source code" is the set
//    of files used by Xilinx's software to generate a .BIT-format FPGA
//    firmware file.
"""

if GPL:
    print """\
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
"""

if LGPL:
    print """\
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
"""

if DUAL_GPL_BSD:
    print """\
//
// This program is is licensed under a disjunctive dual license giving you
// the choice of one of the two following sets of free software/open source
// licensing terms:
//
//    * GNU General Public License (GPL), version 2.0 or later
//    * 3-clause BSD License
// 
//
// The GNU GPL License:
// 
//     This program is free software; you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation; either version 2 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program; if not, write to the Free Software
//     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
// 
// 
// The 3-clause BSD License:
// 
//     Redistribution and use in source and binary forms, with or without
//     modification, are permitted provided that the following conditions
//     are met:
// 
//         * Redistributions of source code must retain the above copyright
//           notice, this list of conditions and the following disclaimer.
// 
//         * Redistributions in binary form must reproduce the above
//           copyright notice, this list of conditions and the following
//           disclaimer in the documentation and/or other materials
//           provided with the distribution.
// 
//         * Neither the name of Mesa Electronics nor the names of its
//           contributors may be used to endorse or promote products
//           derived from this software without specific prior written
//           permission.
// 
// 
// Disclaimer:
// 
//     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//     FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//     COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//     ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//     POSSIBILITY OF SUCH DAMAGE.
// 
"""


filename = args[0]
data = open(filename).read();
index = bit_file_verify_and_print_header(data)
num_bytes = len(data) - index

variable_name = "m7i43_firmware_" + filename_to_c_variable(filename)

print "static unsigned char %s[] = {" % variable_name

for i, byte in enumerate(data[index:]):
    print "%3d," % reverse_bits(byte),
    if i % 16 == 15:
        print
    if i % 4096 == 0:
        sys.stderr.write("\r%10d/%d (%3d%%)" % (i, num_bytes, 100.0 * i / num_bytes))

print "};"
sys.stderr.write("\r%-30s\n" % "done!")

