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

class BaseSymbol:
    def __init__ (self):
	print "called BaseSymbol __init__ (should never happen)"

    def get_modtype (self):
	fields = self.name.split("__")
	if len(fields) < 3:
	    return ""
	return fields[0]

    def get_instnum (self):
	fields = self.name.split("__")
	if len(fields) < 3:
	    return ""
	return fields[1]

    def get_instname (self):
	fields = self.name.split("__")
	if len(fields) < 3:
	    return ""
	return fields[0]+"."+fields[1]

    def get_suffix (self):
	fields = self.name.split("__")
	if len(fields) < 3:
	    return ""
	return fields[2]

    def get_value (self):
	return repr(self.value)

    def __repr__ (self):
	rep = [ self.name, "undefined, base class" ]
	return rep

    def __cmp__ (self, other):
	if self.name > other.name:
	   return 1
	if self.name < other.name:
	   return -1
	return 0

class BoolSymbol(BaseSymbol):
    def __init__ (self, name, tweakable, default, question):
	print "called BoolSymbol __init__ ", name, tweakable, default, question
	self.name = name
	self.default = default
	self.value = default
	self.tweakable = tweakable
	self.question = question

    def __repr__ (self):
	rep = [ self.name, self.value, "bool", self.tweakable, self.default, self.question ]
	return repr(rep)

    def report (self):
	self.value = self.var.get()
	if self.value:
	    return self.question+": YES"
	else:
	    return self.question+": NO"

class EnumSymbol(BaseSymbol):
    def __init__ (self, name, tweakable, default, choices, question):
	print "called EnumSymbol __init__ ", name, tweakable, default, choices, question
	self.name = name
	self.tweakable = tweakable
	self.default = default
	self.value = default
	self.choices = choices
	self.question = question

    def __repr__ (self):
	rep = [ self.name, self.value, "enum", self.tweakable, self.default, self.choices, self.question ]
	return  repr(rep)

    def report (self):
	self.value = self.choices[self.var.get()]
	# in case the value isn't one of the legal choices...
	rep = self.question+": "+str(self.value)
	# but it ought to be, so find it
	for string, val in self.choices.iteritems():
	    if str(val) == str(self.value):
		rep = self.question+": "+string
		break
	return rep


class ConstantSymbol(BaseSymbol):
    def __init__ (self, name, value):
	print "called ConstantSymbol __init__ ", name, value
	self.name = name
	self.value = value

    def __repr__ (self):
	rep = [ self.name, self.value, "constant" ]
	return repr(rep)

    def report (self):
	return str(self.value)

#class PinSymbol(BaseSymbol):
#    def __init__ (self, name, value, spec):
#	self.name = name
#	self.value = value
#	self.tweakable = spec[1]
#	self.default = spec[2]
#	self.description = spec[3]
#
#    def __repr__ (self):
#	rep = [ "pin", self.tweakable, self.default, self.description ]
#
#    def report (self):
#	return self.description+": pin "+str(self.value)


# 'SpecSymbol' is a "class" that creates an instance of a symbol
# based on the spec.  the actual class instance created varies
# depending on the spec, so we implement SpecSymbol as a function
def SpecSymbol(name, spec):
    print "called SpecSymbol ", name, spec
    var_type = spec[0]
    if var_type == "bool":
	return BoolSymbol(name, spec[1], spec[2], spec[3])
    elif var_type == "enum":
	return EnumSymbol(name, spec[1], spec[2], spec[3], spec[4])
    elif var_type == "constant":
	return ConstantSymbol(name, spec[1])
    else:
	print "unknown symbol type '"+var_type+"'"
	return None



def section2dict(section, cfgfile) :
    if not cfgfile.has_section(section) :
	print "ERROR: section '"+section+"' not found"
	sys.exit(1)
    tmp = {}
    for option in cfgfile.options(section) :
	tmp[option] = cfgfile.get(section, option)
    return tmp

class KeyValueError (Exception):
    def __str__ (self):
	return "KeyValueError"

class SymbolSpec :
    def __init__ (self, defstr) :
	#print "SymbolSpec.__init__("+defstr+")"
	definition = eval(defstr)
	try:
	    types = definition["type"].split("_")
	    self.vartype = types[0]
	    if len(types) == 1:
		self.tweakable = 0
	    elif len(types) == 2:
		if types[1] == "preroute":
		    self.tweakable = 0
		elif types[1] == "postroute":
		    self.tweakable = 1
		else:
		    raise KeyValueError, ("type", definition["type"])
	    else:
		raise KeyValueError, ("type", definition["type"])
	    if self.vartype == "constant":
		self.default = definition["value"]
	    elif self.vartype == "pin":
		self.default = -1
		self.direction = definition["direction"]
		self.description = definition["description"]
	    elif self.vartype == "enum":
		self.default = definition["default"]
		self.choices = definition["options"]
		self.question = definition["question"]
	    elif self.vartype == "bool":
		self.default = definition["default"]
		self.question = definition["question"]
	    else:
		print "ERROR: symbol spec '"+defstr+"' has illegal type '"+self.vartype+"'"
		sys.exit(1)
	except KeyError, key:
	    print "ERROR: symbol spec '"+defstr+"' is missing item '"+str(key)+"'"
	    sys.exit(1)
	except KeyValueError, (key, value):
	    print "ERROR: symbol spec '"+defstr+"' has bad value '"+value+"' for item '"+str(key)+"'"
	    sys.exit(1)

    def __repr__ (self):
	rep = [ self.vartype, self.tweakable, self.default ]
	if self.vartype == "constant":
	    pass
	elif self.vartype == "pinnumber":
	    rep.append(self.direction)
	    rep.append(self.description)
	elif self.vartype == "enum":
	    rep.append(self.choices)
	    rep.append(self.question)
	elif self.vartype == "bool":
	    rep.append(self.question)
	return repr(rep)


class ModuleSpec :
    def __init__ (self, name) :
	global lib
	global packages
	#print "ModuleSpec.__init__("+name+")"
	# save basic stuff
	self.name = name
	# extract symbol data from module spec library
	section = name+".symbols"
	if not lib.has_section(section):
	    print "ERROR: module spec '"+name+"' has no 'symbols' section"
	    sys.exit(1)
	self.symspecs = {}
	for key in lib.options(section):
	    self.symspecs[key] = SymbolSpec(lib.get(section, key))
	# check for required symbols
	required_symbols = [ 'num_regs', 'id_code', 'vhdl_package' ]
	for s in required_symbols :
	    if not s in self.symspecs :
		print "ERROR: modspec '"+self.name+"' needs symbol '"+s+"'"
		sys.exit(1)
	# extract template data from module spec library
	section = name+".templates"
	if not lib.has_section(section):
	    print "WARNING: module spec '"+name+"' has no 'templates' section"
	self.templates = {}
	required_templates = [ 'sigs', 'vhdl', 'ram' ]
	for key in required_templates:
	    if lib.has_option(section, key):
		s = lib.get(section, key)
		# strip '!' hackery (used to preserve indenting), and
		# turn it into a Template object for later substitution
		self.templates[key] = string.Template(re.sub("(?m)^!","", s))
	    else:
		# load default template
		self.templates[key] = string.Template("")
	# do we already have the neccessary package?
	p = self.symspecs["vhdl_package"].default
	if not p in packages :
	    packages.append(p)
	# turn strings into integers
	self.num_regs = int(self.symspecs["num_regs"].default)
	self.id_code = int(self.symspecs["id_code"].default)
	# compute power of two size of register block
	self.blk_size = 1
	while self.blk_size < self.num_regs :
	    self.blk_size <<= 1

    def to_list (self) :
	rep = [ self.name, self.id_code, self.num_regs, self.blk_size, self.symspecs]
	templates = {}
	for key, template in self.templates.iteritems():
	    templates[key] = template.template
	rep.extend([templates])
	return rep

    def __repr__ (self) :
	return repr(self.to_list())

    def symbol_table (self) :
	rv = {}
	for name,value in self.symspecs.iteritems() :
	    name = self.name+"__"+name
	    rv[name] = value
	return rv

# source select logic depends on how many sources there are

sel = []
sel.append("""${o} <= (${i0})""")
sel.append("""${o} <=
 (${i0} and (not ${s0})) or
 (${i1} and      ${s0}) """)
sel.append("""${o} <=
 (${i0} and (not ${s0}) and (not ${s1})) or
 (${i1} and      ${s0}  and (not ${s1})) or
 (${i2} and                      ${s1} )""")
sel.append("""${o} <=
 (${i0} and (not ${s0}) and (not ${s1})) or
 (${i1} and      ${s0}  and (not ${s1})) or
 (${i2} and (not ${s0}) and      ${s1} ) or
 (${i3} and      ${s0}  and      ${s1} )""")
sel.append("""${o} <=
 (${i0} and (not ${s0}) and (not ${s1}) and (not ${s2})) or
 (${i1} and      ${s0}  and (not ${s1}) and (not ${s2})) or
 (${i2} and (not ${s0}) and      ${s1}  and (not ${s2})) or
 (${i3} and      ${s0}  and      ${s1}  and (not ${s2})) or
 (${i4} and                                      ${s2} )""")
sel.append("""${o} <=
 (${i0} and (not ${s0}) and (not ${s1}) and (not ${s2})) or
 (${i1} and      ${s0}  and (not ${s1}) and (not ${s2})) or
 (${i2} and (not ${s0}) and      ${s1}  and (not ${s2})) or
 (${i3} and      ${s0}  and      ${s1}  and (not ${s2})) or
 (${i4} and (not ${s0}) and                      ${s2} ) or
 (${i5} and      ${s0}  and                      ${s2} )""")
sel.append("""${o} <=
 (${i0} and (not ${s0}) and (not ${s1}) and (not ${s2})) or
 (${i1} and      ${s0}  and (not ${s1}) and (not ${s2})) or
 (${i2} and (not ${s0}) and      ${s1}  and (not ${s2})) or
 (${i3} and      ${s0}  and      ${s1}  and (not ${s2})) or
 (${i4} and (not ${s0}) and (not ${s1}) and      ${s2} ) or
 (${i5} and      ${s0}  and (not ${s1}) and      ${s2} ) or
 (${i6} and                      ${s1}  and      ${s2} )""")
sel.append("""${o} <=
 (${i0} and (not ${s0}) and (not ${s1}) and (not ${s2})) or
 (${i1} and      ${s0}  and (not ${s1}) and (not ${s2})) or
 (${i2} and (not ${s0}) and      ${s1}  and (not ${s2})) or
 (${i3} and      ${s0}  and      ${s1}  and (not ${s2})) or
 (${i4} and (not ${s0}) and (not ${s1}) and      ${s2} ) or
 (${i5} and      ${s0}  and (not ${s1}) and      ${s2} ) or
 (${i6} and (not ${s0}) and      ${s1}  and      ${s2} ) or
 (${i7} and      ${s0}  and      ${s1}  and      ${s2} )""")


# a PinDriver represents one I/O pin
#
# create by invoking PinDriver(instnum)
#
# public methods:
#    add_source(output, enable) - register a source signal for the pin
#	'output' and optionally 'enable' are the names of signals provided
#	elsewhere.  if 'enable' is an empty string, the output will always
#	be enabled.
#    vhdl_signal_decl() - returns VHDL declarations for any global
#	signals that pin driver needs
#    vhdl_logic() - returns a VHDL description of the pin driver
#
#    assign_address(cslist) - creates a ChipSelect object (if needed)
#    variables() - returns a list of the variables used by the module
#    ram_template() - returns a list of python expressions, one per byte
#	the expressions may use variables as defined by the variables method
#
#
#
#
# data members (private):
#
#
#

class PinDriver :
    def __init__ (self, number) :
	print "PinDriver.__init__("+str(number)+")"
	# save some basic stuff
	self.number = number
	self.num_sources = 1
	self.sources = []

    def add_source(self, output, enable) :
	print "PinDriver."+str(self.number)+".add_source("+str(output)+", "+str(enable)+")"
	if len(self.sources) >= 8 :
	    print "too many sources for pin number ", self.number
	    sys.exit(2)
	self.sources.append((output, enable))

    def vhdl_signal_decl (self) :
	signals = "-- no signals for pin "+self.number+"\n"
	return signals

    def vhdl_logic (self) :
	try:
	    logic = self.module.templates["vhdl"].substitute(self.symbols)
	except KeyError :
	    print "ERROR: no value for '"+str(sys.exc_value)+"' in '"+self.name+"'"
	    sys.exit(1)
	return logic

    def ram_template (self) :
	# use "__" to separate module, instance, and variable names
	prefix = "${"+self.modname+"__"+self.instance+"__"
	rv = self.module.templates["ram"].template
	rv = re.sub("\${",prefix, rv)
	return rv

    def symbol_table (self) :
	rv = {}
	for name,value in self.symbols.iteritems() :
	    name = self.modname+"__"+self.instance+"__"+name
	    rv[name] = value
	return rv




# a ModuleInstance represents a block of logic inside the FPGA
#
# create by invoking ModuleInstance(modspec, instnum)
#
# public methods:
#    assign_address(cslist) - creates a ChipSelect object (if needed)
#    vhdl_signal_decl() - returns VHDL declarations for any global
#	signals that the module needs
#    vhdl_logic() - returns a VHDL component invocation for the module
#    variables() - returns a list of the variables used by the module
#    ram_template() - returns a list of python expressions, one per byte
#	the expressions may use variables as defined by the variables method
#
# data members (private):

class ModuleInstance :
    def __init__ (self, name) :
	global src
	global modspecs
	global symbols
	print "ModuleInstance.__init__("+name+")"
	# save some basic stuff
	self.name = name
	self.modname = name.split('.')[0]
	self.instance = name.split('.')[1]
	# do we already have a matching module spec?
	if not self.modname in modspecs :
	    # no, add the module spec
	    modspecs[self.modname] = ModuleSpec(self.modname)
	self.module = modspecs[self.modname]
	# get default and/or constant symbol values from module spec
	self.symbols = {}
	for key,symspec in self.module.symspecs.iteritems():
	    self.symbols[key] = symspec.default
	# get variable/value pairs from the spec file (replace defaults)
	for key in src.options(name) :
	    if key in self.symbols.keys():
		self.symbols[key] = src.get(name, key)
	    else:
		print "WARNING: module '"+name+"' has unneeded variable '"+key+"'"
	# add a couple special values
	self.symbols["instnum"] = self.instance
	self.byteaddr = -1

    def to_list (self) :
	rep = [ self.name, self.modname, self.instance, self.symbols]
	return rep

    def __repr__ (self) :
	return repr(self.to_list())

    def get_name (self) :
	return self.name

    def get_num_regs (self) :
	return self.module.num_regs

    def get_id_code_and_instance (self) :
	return (self.module.id_code << 8 ) | int(self.instance)

    def get_blk_size (self) :
	return self.module.blk_size

    def assign_address (self, addr) :
	self.wordaddr = upper_limit
	self.byteaddr = upper_limit * 4
	self.symbols['baseaddr'] = repr(self.byteaddr)
	mask = 0x3FFF & ~(self.module.blk_size-1)
	self.select = ChipSelect(self.wordaddr, mask, chip_selects)
	self.symbols['cs'] = self.select.name
	#chip_selects.append(self.select)

    def vhdl_signal_decl (self) :
	signals = "-- no signals for "+self.name+"\n"
	return signals

    def vhdl_logic (self) :
	try:
	    logic = self.module.templates["vhdl"].substitute(self.symbols)
	except KeyError :
	    print "ERROR: no value for '"+str(sys.exc_value)+"' in '"+self.name+"'"
	    sys.exit(1)
	return logic

    def ram_template (self) :
	# use "__" to separate module, instance, and variable names
	prefix = "${"+self.modname+"__"+self.instance+"__"
	rv = self.module.templates["ram"].template
	rv = re.sub("\${",prefix, rv)
	return rv

    def symbol_table (self) :
	rv = {}
	for name,value in self.symbols.iteritems() :
	    name = self.modname+"__"+self.instance+"__"+name
	    rv[name] = value
	return rv

class GPIOInstance :
    def __init__ (self, name) :
	global src
	global modspecs
	print "GPIOInstance.__init__("+name+")"
	# save some basic stuff
	self.name = name
	self.modname = name.split('.')[0]
	self.instance = name.split('.')[1]
	# set constant symbol values
	# GPIO doesn't use a module spec, the data is hardcoded here
	self.symbols = {}
	self.symbols["id_code"] = 1
	self.symbols["num_regs"] = 7
	self.symbols["instnum"] = self.instance
	self.byteaddr = -1

    def to_list (self) :
	rep = [ self.name, self.modname, self.instance, self.symbols]
	return rep

    def __repr__ (self) :
	return repr(self.to_list())

    def get_name (self) :
	return self.name

    def get_num_regs (self) :
	return self.module.num_regs

    def get_id_code_and_instance (self) :
	return (self.module.id_code << 8 ) | int(self.instance)

    def get_blk_size (self) :
	return self.module.blk_size

    def assign_address (self, addr) :
	self.wordaddr = upper_limit
	self.byteaddr = upper_limit * 4
	self.symbols['baseaddr'] = repr(self.byteaddr)
	mask = 0x3FFF & ~(self.module.blk_size-1)
	self.select = ChipSelect(self.wordaddr, mask, chip_selects)
	self.symbols['cs'] = self.select.name

    def vhdl (self) :
	try:
	    vhdl = self.module.templates["vhdl"].substitute(self.symbols)
	except KeyError :
	    print "ERROR: no value for '"+str(sys.exc_value)+"' in '"+self.name+"'"
	    sys.exit(1)
	return vhdl

    def ram_template (self) :
	# use "__" to separate module, instance, and variable names
	prefix = "${"+self.modname+"__"+self.instance+"__"
	rv = self.module.templates["ram"].template
	rv = re.sub("\${",prefix, rv)
	return rv

    def symbol_table (self) :
	rv = {}
	for name,value in self.symbols.iteritems() :
	    name = self.modname+"__"+self.instance+"__"+name
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

# a ChipSelect object represents a block of address space, and
# implements a VHDL signal that goes true for any address in that
# block
#
# create by invoking ChipSelect(address, mask, cslist)
#    addr   = base address (in 32-bit words)
#    mask   = ones indicate bits to be decoded, zeros are no-care
#    cslist = list of chip selects - the list will be consulted for
#		higher level selects that can be re-used to save logic,
#		and the newly created select(s) will be added to the
#		list for possible reuse
#
# public methods:
#    signal_name() - returns a VHDL identifier that is the chip select signal name
#    vhdl_signal_decl() - returns a VHDL declaration for the chip select signal
#    vhdl_logic() - returns a VHDL logic statement that implements the select
#
# data members (private):
#    addr = base address (in 32-bit words)
#    mask = identifies the no-care bits in the address
#    name = a string, like "cs0010xxx01011xx"
#    version = 0 if 4 (or fewer) address lines into a LUT
#	       1 if 1 higher level select and 3 (or fewer) address lines 
#	       3 if 2 higher level selects and 2 (or fewer) address lines
#	      (Higher level selects are created if needed, all selects are
#	       assumed to be in the list "chip_selects[]")
#    finalmask = mask of address lines used at this stage (4 or less)
#    ena1name = name of upper level select (if version 1 or 3)
#    ena2name = name of 2nd upper level select (if version 3)

class ChipSelect :
    def __init__ (self, addr, mask, cslist) :
	print "ChipSelect.__init__("+binstr(addr)+", "+binstr(mask)+")"
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
	for s in cslist :
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
	    upper1 = ChipSelect(addr, umask, cslist)
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
	for s in cslist :
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
	    upper2 = ChipSelect(addr, umask, cslist)
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
	# the signal name
	return self.name

    def vhdl_signal_decl (self) :
	# the VHDL signal declaration
	return "\tsignal "+self.name+" : std_logic;\n"

    def vhdl_logic (self) :
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


# start of program - parse command line
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
# read the source .spec file into a config file object
src = ConfigParser.ConfigParser()
src.read(spec_fname)
if len(src.sections()) == 0 :
    print "ERROR: source file '"+spec_fname+"' not found, empty, or misformatted"
    sys.exit(2)

pins = []
pins.append(PinDriver(0))
pins.append(PinDriver(1))
pins[0].add_source("out0", "ena0")
pins[0].add_source("out1", "")
pins[0].add_source("out2", "")
pins[0].add_source("out3", "")
pins[0].add_source("out4", "foo")
pins[0].add_source("out5", "")
pins[0].add_source("out6", "bar")
pins[0].add_source("out7", "")
pins[0].add_source("out8", "")
pins[0].add_source("out9", "")
sys.exit(2)

# read the [global] section of the spec
try:
    board = src.get("global", "board")
except ConfigParser.NoOptionError, inst:
    print "ERROR: "+spec_fname+": "+str(inst)
    sys.exit(2)
# deal with board specific stuff
if board == "5i20":
    num_ports = 3
    pins_per_port = 24
    num_pins = num_ports * pins_per_port
    device = "2s200pq208-5"
    constraints = "5i20-normal.ucf"
elif board == "5i22":
    print "ERROR: the 5i22 board is not yet supported"
    sys.exit(2)
else:
    print "ERROR: '"+board+"' is not valid for [global]board"
    sys.exit(2)


# get a list of all module spec files in the library
libpath += "/*.mspec"
mspecs = glob.glob(libpath)
if len(mspecs) == 0 :
    print "ERROR: no module specs found at '"+libpath+"'"
    sys.exit(2)
# read the module spec file(s) into a config file object
lib = ConfigParser.ConfigParser()
for name in mspecs :
    lib.read(name)

# create empty data structures
symbols = []
instances = []
modspecs = {}
packages = []

# DEBUG ONLY
num_ports = 2
pins_per_port = 2

# create symbols for the pins
portnames = "ABCDEF"
for p in range(num_ports):
    for n in range(pins_per_port):
	pin_name = "pin__"+portnames[p]+"%02d"%n+"__"
	print "creating pin ", pin_name
	symbols.append(EnumSymbol(pin_name+"source", 1, 0, { "gpio":0 }, "source for pin" ))
	symbols.append(EnumSymbol(pin_name+"mode", 1, 0, { "disabled":0, "enabled":1, "tri-state":2, "open-collector":3 }, "output mode" ))
	symbols.append(EnumSymbol(pin_name+"polarity", 1, 0, { "active high":0, "active low":1 }, "polarity" ))
	symbols.append(BoolSymbol(pin_name+"export-input", 1, 0, "Export HAL input pin" ))

# create module instances for the GPIO ports
#for n in range(num_ports):
#    print "creating port ", n
#    instances.append(ModuleInstance("gpio."+str(n)))
# create instances of VHDL modules based on spec file
for name in src.sections() :
    if name == "global":
	# the [global] section doesn't define a module
	continue
    print "creating instance ", name
    instances.append(ModuleInstance(name))


# assign addresses to instances
# size of address space: 16K 32-bit words, with the
# first 256 words (1K bytes) reserved for config RAM
upper_limit = 0x4000
lower_limit = 0x0100
chip_selects = []
# chip select for the RAM
ram_chip_select = ChipSelect(0,0x3F00, chip_selects)
# need a chip select for every "<module>__<instance>__num_regs" variable
needed = []
for symbol in symbols:
    suffix = symbol.get_suffix()
    if suffix == "num_regs":
	regs = int(symbol.get_value())
	if regs <= 0 :
	    # no registers, doesn't need address space or chip select
	    break
	# calculate address space size (power of two)
	size = 1
	while size < regs:
	    size <<= 1
	# add item to list of things that need chip-selects
	# each entry is a a list: [ name, size, type, instance number ]
	needed.append( [ symbol.get_instname(), size, symbol.get_modtype(), symbol.get_instnum() ] )
# multi-stage sort
# result is sorted by size (largest first), then type, then instance number
needed.sort(key=lambda i : int(i[3]))
needed.sort(key=lambda i : i[2])
needed.sort(key=lambda i : i[1], reverse=1)
# address spaces are assigned from largest to smallest, since
# larger spaces have more demanding alignment requirements
for i in needed :
    # selects are placed starting at the top of the address
    # space, since the RAM block at the bottom would mess
    # up alignment of anything bigger than 256 registers
    print i
    upper_limit -= i[1]
    if upper_limit < lower_limit :
	print "ERROR: ran out of address space in FPGA"
	sys.exit(1)
	self.byteaddr = upper_limit * 4
	self.symbols['baseaddr'] = repr(self.byteaddr)
	mask = 0x3FFF & ~(self.module.blk_size-1)
	self.select = ChipSelect(self.wordaddr, mask, chip_selects)
	self.symbols['cs'] = self.select.name

    #i.assign_address(upper_limit)
# sort so that derived selects come after the ones they use
# (not strictly neccessary, VHDL can figure it out anyway)
chip_selects.sort()
# restore "sorted by ID code and instance" ordering for
# subsequent processing
instances.sort(key=ModuleInstance.get_id_code_and_instance)





# more gathering of infomation and validation needs to be done here


# start generating output, top level VHDL file first
# prepare for substitution into top-level VHDL file
toplevel_values = {}
toplevel_values["outfile"] = vhdl_fname
toplevel_values["preprocessor"] = sys.argv[0]
toplevel_values["infile"] = spec_fname
toplevel_values["timestamp"] =  str(datetime.datetime.now()).split(".")[0]
toplevel_values["device"] = device
toplevel_values["constraints"] = constraints
toplevel_values["packages"] = ""
for p in packages :
    toplevel_values["packages"] += "use work."+p+"_pkg.all;\n"

toplevel_values["instance_signals"] = ""
toplevel_values["instance_logic"] = ""
for i in instances :
    toplevel_values["instance_signals"] += i.vhdl_signal_decl()
    toplevel_values["instance_logic"] += i.vhdl_logic()

toplevel_values["chipselect_signals"] = ""
toplevel_values["chipselect_logic"] = ""
for cs in chip_selects :
    toplevel_values["chipselect_signals"] += cs.vhdl_signal_decl()
    toplevel_values["chipselect_logic"] += cs.vhdl_logic()

# do the substitution and write output file
toplevel_in = open("toplevel.vhd", "r")
toplevel_out = open(vhdl_fname, "w")
toplevel_out.write(string.Template(toplevel_in.read()).substitute(toplevel_values))

# next we generate the data to be merged into the final .fpga file
ram_template = ""
symbol_values = {}
#print instances
for i in instances:
    ram_template += i.ram_template()
    symbol_values.update(i.symbol_table())
symbol_specs = {}
for m in modspecs.values():
    symbol_specs.update(m.symbol_table())

bf = bitfile.BitFile()
# 't' section - template for RAM
bf["t"] = repr(ram_template)
# 'v' section - symbol values
bf["v"] = repr(symbol_values)
# 's' section - symbol specs
bf["s"] = repr(symbol_specs)
# write to file
bf.tofilename(rspec_fname)

