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
import os
import glob
import ConfigParser
import string
import re
import datetime
import getopt
# this is a hack until I find out how we're really supposed to do it
sys.path.append("../../../../../lib/python")
import bitfile

def section2dict(section, cfgfile) :
    if not cfgfile.has_section(section) :
	print "ERROR: section '"+section+"' not found"
	sys.exit(1)
    tmp = {}
    for option in cfgfile.options(section) :
	tmp[option] = cfgfile.get(section, option)
    return tmp

class ModuleSpec :
    def __init__ (self, name) :
	global lib_cfgfile
	global packages
	if not lib_cfgfile.has_section(name+".constants") :
	    print "ERROR: no module spec found for '"+name+"'"
	    sys.exit(1)
	# save basic stuff
	self.name = name
	# extract data from config file object 'lib'
	self.constants = section2dict(name+".constants", lib_cfgfile)
	self.pins = section2dict(name+".pins", lib_cfgfile)
	self.pre_vhdl_vars = section2dict(name+".pre-vhdl-vars", lib_cfgfile)
	self.post_vhdl_vars = section2dict(name+".post-vhdl-vars", lib_cfgfile)
	# get required templates
	section = name+".templates"
	if not lib_cfgfile.has_section(section) :
	    print "ERROR: module spec '"+name+"' missing 'templates' section"
	    sys.exit(1)
	self.templates = {}
	for template in [ 'sigs', 'vhdl', 'ram' ] :
	    if not lib_cfgfile.has_option(section, template) :
		print "ERROR: module spec '"+name+"' missing '"+template+"' template"
		sys.exit(1)
	    # get value
	    s = lib_cfgfile.get(section, template)
	    # strip '!' hackery (used to preserve indenting in templates)
	    s = re.sub("(?m)^!","",s)
	    # turn it into a Template object for later substitution
	    self.templates[template] = string.Template(s)
	# check for required constants
	for constant in [ 'num_regs', 'id_code', 'vhdl_package' ] :
	    if not constant in self.constants :
		print "ERROR: modspec '"+self.name+"' needs constant '"+constant+"'"
		sys.exit(1)
	# do we already have the neccessary package?
	p = self.constants['vhdl_package']
	if not p in packages :
	    packages.append(p)
	# turn string into integer
	self.num_regs = eval(self.constants['num_regs'])
	# compute power of two size of register block
	self.blk_size = 1
	while self.blk_size < self.num_regs :
	    self.blk_size <<= 1

    def __str__ (self) :
	return self.name

    def __repr__ (self) :
	return self.name



class ModuleInstance :
    def __init__ (self, name) :
	global src_cfgfile
	global modspecs
	# save some basic stuff
	self.name = name
	self.modname = name.split('.')[0]
	self.instance = name.split('.')[1]
	# extract variable/value pairs from config file object 'src'
	self.values = section2dict(name, src_cfgfile)
	# add a couple values
	self.values["instnum"] = self.instance
	# do we already have a matching module spec?
	if not self.modname in modspecs :
	    # no, add the module spec
	    modspecs[self.modname] = ModuleSpec(self.modname)
	self.module = modspecs[self.modname]
	# add module spec supplied constants to this instance
	for cname,cvalue in self.module.constants.iteritems() :
	    self.values[cname] = cvalue
	# verify that we have values for every variable in the module spec
	required = self.module.pins.keys()
	required.extend(self.module.pre_vhdl_vars.keys())
	required.extend(self.module.post_vhdl_vars.keys())
	for variable in required :
	    if not self.values.has_key(variable) :
		print "ERROR: instance '"+self.name+"' needs a value for '"+variable+"'"
		sys.exit(1)

    def __str__ (self) :
	return self.name

    def __repr__ (self) :
	return self.name

    def get_name (self) :
	return self.name

    def get_num_regs (self) :
	return self.module.num_regs

    def get_blk_size (self) :
	return self.module.blk_size

    def assign_address (self, addr) :
	self.wordaddr = upper_limit
	self.byteaddr = upper_limit * 4
	self.values['baseaddr'] = repr(self.byteaddr)
	mask = 0x3FFF & ~(self.module.blk_size-1)
	self.select = ChipSelect(self.wordaddr, mask)
	self.values['cs'] = self.select.name
	chip_selects.append(self.select)

    def vhdl (self) :
	vhdl = self.module.templates["vhdl"].substitute(self.values)
	return vhdl

    def ram_template (self) :
	prefix = "${"+self.modname+"_"+self.instance+"_"
	rv = self.module.templates["ram"].template
	rv = re.sub("\${",prefix, rv)
	return rv

    def variable_values (self) :
	rv = {}
	for name,value in self.values.iteritems() :
	    name = self.modname+"_"+self.instance+"_"+name
	    rv[name] = value
	return rv


def binstr(value) :
    name = ""
    for n in reversed(range(14)) :
	if (value & (1<<n)) :
	    name += '1'
	else :
	    name += '0'
    return name

def count_ones(value) :
    ones = 0
    for n in reversed(range(14)) :
	if (value & (1<<n)) :
	    ones += 1
    return ones

def trim_ones_lsb(value, bits) :
    for n in range(14) :
	if (value & (1<<n)) :
	    value &= ~(1<<n)
	    bits -= 1
	    if bits == 0 :
		break
    return value

class ChipSelect :
    def __init__ (self, addr, mask) :
	self.addr = addr
	self.mask = mask
	self.name = "cs"
	for n in reversed(range(14)) :
	    if (mask & (1<<n)) :
		if (addr & (1<<n)) :
		    self.name += '1'
		else :
		    self.name += '0'
	    else :
		self.name += 'x'
	if count_ones(mask) <= 4 :
	    # can decode with a single 4 bit LUT
	    self.version = 0
	    self.finalmask = mask
	    return
	# need multiple level decode
	# find best available upper level decode
	mask1 = mask
	for s in chip_selects :
	    m = s._remainder(addr, mask)
	    if ( m < mask1 ) :
		upper1 = s
		mask1 = m
	if ( mask1 == mask ) or ( count_ones(mask1) > 9 ) :
	    # didn't find a suitable decode, need to make one
	    # these values are magic, designed to result in no more than
	    # three levels of logic, and no more than four inputs at each level
	    lut = { 5:3, 6:3, 7:3, 8:5, 9:5, 10:6, 11:3, 12:3, 13:3, 14:8, 15:8, 16:9 }
	    umask = trim_ones_lsb(mask, lut[count_ones(mask)])
	    upper1 = ChipSelect(addr, umask)
	    chip_selects.append(upper1)
	    mask1 = upper1._remainder(addr, mask)
	if count_ones(mask1) <= 3 :
	    # can decode with a single upper level
	    self.version = 1
	    self.ena1name = upper1.name
	    self.finalmask = mask1
	    return
	# need another upper level decode
	# find best available upper level decode
	mask2 = mask1
	for s in chip_selects :
	    m = s._remainder(addr, mask1)
	    if count_ones(m) < count_ones(mask2) :
		upper2 = s
		mask2 = m
	    elif ( count_ones(m) == count_ones(mask2)) and ( m < mask2 ) :
		upper2 = s
		mask2 = m
	if count_ones(mask2) > 2 :
	    # didn't find a suitable decode, need to make one
	    # we want to have 2 bits left to decode
	    umask = trim_ones_lsb(mask1, 2)
	    upper2 = ChipSelect(addr, umask)
	    chip_selects.append(upper2)
	    mask2 = upper2._remainder(addr, mask1)
	# combine the two upper decodes and 2 bits of address
	self.version = 3
	self.ena1name = upper1.name
	self.ena2name = upper2.name
	self.finalmask = mask2
	return

    # return the bits in the 'addr, mask' block that
    # this chipselect object does not decode
    def _remainder(self, addr, mask) :
	if ( self.mask & addr ) != ( self.mask & self.addr ) :
	    # wrong address
	    return mask
	if ( self.mask & mask ) != self.mask :
	    # too specific
	    return mask
	return mask & ~self.mask

    def __str__ (self) :
	return self.name

    def __repr__ (self) :
	return self.name

    def __cmp__ (self, other) :
	# ordering is such that upper level selects come
	# before lower level ones that (might) use them
	smask = self.mask
	for n in reversed(range(14)) :
	    if (smask & (1<<n)) : break
	    smask |= (1<<n)
	omask = other.mask
	for n in reversed(range(14)) :
	    if (omask & (1<<n)) : break
	    omask |= (1<<n)
	saddr = self.addr & smask
	oaddr = other.addr & omask
	if saddr < oaddr : return -2
	if saddr > oaddr : return  2
	if smask < omask : return -1
	if smask > omask : return  1
	return 0

    def signal (self) :
	# the VHDL signal declaration
	return "\tsignal "+self.name+" : std_logic;\n"

    def logic (self) :
	# the VHDL implementation
	logic = "    "+self.name+" <= "
	if self.version & 1 :
	    logic += " and "+self.ena1name
	if self.version & 2 :
	    logic += " and "+self.ena2name
	for n in reversed(range(14)) :
	    if (self.finalmask & (1<<n)) :
		logic += " and "
		if (self.addr & (1<<n)) :
		    logic += "addr("+str(n+2)+")"
		else :
		    logic += "not(addr("+str(n+2)+"))"
	logic += ";\n"
	# its easier to put 'and' in front of every term and delete
	# the first one later, than to figure out which term is first
	logic = logic.replace(" and ","",1)
	return logic


def usage ():
    print "\nUsage: spec2vhdl [options] <name>\n"
    print "  Reads <name>.spec, writes <name>.rspec and <name>.vhd, where"
    print "  <name>.spec is an FPGA spec, <name>.rspec is a config RAM data"
    print "  spec (used later in the build process), and <name>.vhd is a top"
    print "  level VHDL file that implements the specfied FPGA.\n"
    print "  Options:"
    print "    -s <spec_name>    use <spec_name> as input file"
    print "    -r <rspec_name>   use <rspec_name> for RAM spec output"
    print "    -v <vhdl_name>    use <vhdl_name> for VHDL output"
    print "    -l <lib_path>     read module specs from <lib_path>"
    print "    -h                print this help screen"


try:
    opts, args = getopt.gnu_getopt(sys.argv[1:],"hs:r:v:l:")
except getopt.GetoptError:
    usage()
    sys.exit(2)
if len(args) > 1 :
    usage()
    sys.exit(2)
libpath = "."
if len(args) == 1 :
    spec_fname = args[0]+".spec"
    rspec_fname = args[0]+".rspec"
    vhdl_fname = args[0]+".vhd"
else :
    spec_fname = ""
    rspec_fname = ""
    vhdl_fname = ""
for opt,value in opts :
    if opt == '-h' :
	usage()
	sys.exit(2)
    if opt == "-s" :
	spec_fname = value
    if opt == "-r" :
	rspec_fname = value
    if opt == "-v" :
	vhdl_fname = value
    if opt == "-l" :
	libpath = value
for name in spec_fname, rspec_fname, vhdl_fname :
    if len(name) == 0 :
	usage()
	sys.exit(2)
# read the source file into a config file object
src_cfgfile = ConfigParser.ConfigParser()
src_cfgfile.read(spec_fname)
if len(src_cfgfile.sections()) == 0 :
    print "ERROR: source file '"+spec_fname+"' not found, empty, or misformatted"
    sys.exit(2)
# get a list of all module spec files in the library
libpath += "/*.mspec"
mspecs = glob.glob(libpath)
if len(mspecs) == 0 :
    print "ERROR: no module specs found at '"+libpath+"'"
    sys.exit(2)

# read the module spec file(s) into a config file object
lib_cfgfile = ConfigParser.ConfigParser()
for name in mspecs :
    lib_cfgfile.read(name)

# create empty data structures
instances = []
modspecs = {}
packages = []
chip_selects = []

# create instances of VHDL modules based on spec file
for name in src_cfgfile.sections() :
    instances.append(ModuleInstance(name))

# assign addresses to instances
# size of address space: 16K 32-bit words, with the
# first 256 words (1K bytes) reserved for config RAM
upper_limit = 0x4000
lower_limit = 0x0100
# chip select for the RAM
chip_selects.append(ChipSelect(0,0x3F00))
# preliminary sort gets like modules together, and instances
# in numerical order
instances.sort(key=ModuleInstance.get_name, reverse=1)
# instances are placed from largest to smallest, since larger
# address spaces have more demanding alignment requirements
instances.sort(key=ModuleInstance.get_num_regs, reverse=1)
for i in instances :
    if i.get_num_regs() == 0 :
	# no registers, doesn't need address space or chip select
	break
    # instances are placed starting at the top of the address
    # space, since the RAM block at the bottom would mess
    # up alignment of anything bigger than 256 registers.
    upper_limit -= i.get_blk_size()
    if upper_limit < lower_limit :
	print "ERROR: ran out of address space in FPGA"
	sys.exit(1)
    i.assign_address(upper_limit)
# sort so that derived selects come after the ones they use
# (not strictly neccessary, VHDL can figure it out anyway)
chip_selects.sort()
# restore "sorted by name" ordering for subsequent processing
instances.sort(key=ModuleInstance.get_name)

# more gathering of infomation and validation needs to be done here


# start generating output, top level VHDL file first
# prepare for substitution into top-level VHDL file
toplevel_values = {}
toplevel_values["outfile"] = vhdl_fname
toplevel_values["preprocessor"] = sys.argv[0]
toplevel_values["infile"] = spec_fname
toplevel_values["timestamp"] =  str(datetime.datetime.now()).split(".")[0]
toplevel_values["packages"] = ""
for p in packages :
    toplevel_values["packages"] += "use work."+p+"_pkg.all;\n"
toplevel_values["instance_vhdl"] = ""
for i in instances :
    toplevel_values["instance_vhdl"] += i.vhdl()
toplevel_values["chipselect_signals"] = ""
toplevel_values["chipselect_logic"] = ""
for cs in chip_selects :
    toplevel_values["chipselect_signals"] += cs.signal()
    toplevel_values["chipselect_logic"] += cs.logic()

# do the substitution and write output file
toplevel_in = open("toplevel.vhd", "r")
toplevel_out = open(vhdl_fname, "w")
toplevel_out.write(string.Template(toplevel_in.read()).substitute(toplevel_values))

# next we generate the data to be merged into the final .fpga file
ram_template = ""
variable_values = {}
for i in instances :
    ram_template += i.ram_template()
    variable_values.update(i.variable_values())

bf = bitfile.BitFile()
# 't' section - template for RAM
bf["t"] = repr(ram_template)
# 'v' section - variable values
bf["v"] = repr(variable_values)
bf.tofilename(rspec_fname)

