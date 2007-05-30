#!/usr/bin/python2.4
#    Copyright 2007 John Kasunich
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import sys
import string
# this is a hack until I find out how we're really supposed to do it
sys.path.append("../../../../../lib/python")
import bitfile

# this program takes a .bit file (output from Xilinx toolchain
# and merges it with a .rspec file (output from spec2vhdl) to
# create a .fpga file (input to bfload)

xilinx = bitfile.BitFile.fromfilename("myfile.bit")
rspec = bitfile.BitFile.fromfilename("myfile.rspec")
fpga = bitfile.BitFile()
# copy stuff from Xilinx bitfile
fpga["a"] = xilinx["a"]
fpga["b"] = xilinx["b"]
fpga["c"] = xilinx["c"]
fpga["d"] = xilinx["d"]
fpga["e"] = xilinx["e"]
# copy stuff from .rspec file
fpga["t"] = rspec["t"]
fpga["v"] = rspec["v"]

# get ram template from .rspec file
ram_template = string.Template(eval(rspec["t"]))
# get variable values from .rspec file
variable_values = eval(rspec["v"])
# perform text substitution
text = ram_template.substitute(variable_values)
# strip whitespace, break into lines
text = text.strip()
lines = text.split("\n")
# ram header info goes here
ramdata = []
# evaluate each line to get a byte value
for line in lines :
    ramdata.append(eval(line) & 0x00FF)
# pad with zeros out to 1K - 2 ( save two bytes for checksum )
z = [0]
ramdata.extend(z*(1022-len(ramdata)))
# compute the checksum
checksum1 = 0
checksum2 = 0
for byte in ramdata :
    checksum1 += byte
    checksum1 = checksum1 % 251
    checksum2 += checksum1
    checksum2 =  checksum2 % 251
ramdata.append(checksum1)
ramdata.append(checksum2)
# convert from list of values to string of bytes
bytes = ""
for byte in ramdata :
    bytes += chr(byte)
# add to output file as 'r' chunk - this is what the FPGA loader reads
fpga["r"] = bytes
# and write the file
fpga.tofilename("myfile.fpga")



