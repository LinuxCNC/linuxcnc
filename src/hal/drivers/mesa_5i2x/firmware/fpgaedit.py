#!/usr/bin/env python2.4
#    Copyright 2007 Johm Kasunich jmkasunich AT fastmail DOT fm
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
import os.path
sys.path.append("../../../../../lib/python")
import string
import bitfile
import Tkinter
import bwidget
import tkMessageBox
#import tkColorChooser
import tkFileDialog

class BaseSymbol:
    def __init__ (self, name, value, spec):
	print "called BaseSymbol __init__ ?!?", name, value, spec

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

    def __repr__ (self):
	rep = [ self.name, "undefined" ]
	return rep

    def __cmp__ (self, other):
	if self.name > other.name:
	   return 1
	if self.name < other.name:
	   return -1
	return 0


class BoolSymbol(BaseSymbol):
    def __init__ (self, name, value, spec):
	self.name = name
	self.value = value
	self.tweakable = spec[1]
	self.default = spec[2]
	self.question = spec[3]
	self.var = Tkinter.IntVar()

    def __repr__ (self):
	self.value = self.var.get()
	rep = [ "bool", self.tweakable, self.default, self.question, self.value ]

    def report (self):
	self.value = self.var.get()
	if self.value:
	    return self.question+": YES"
	else:
	    return self.question+": NO"

    def make_widget(self, window, rownum):
	l = Tkinter.Label(window, text="   "+self.question)
	l.grid(row=rownum, column=0, sticky="w")
	self.var.set(self.value)
	if self.tweakable:
	    c = Tkinter.Checkbutton(window, variable=self.var)
	    c.grid(row=rownum, column=1, sticky="w")
	else:
	    if self.value:
		l = Tkinter.Label(window, text="Yes")
	    else:
		l = Tkinter.Label(window, text="No")
	    l.grid(row=rownum, column=1, sticky="w")
	return rownum+1

class EnumSymbol(BaseSymbol):
    def __init__ (self, name, value, spec):
	self.name = name
	self.value = value
	self.tweakable = spec[1]
	self.default = spec[2]
	self.choices = spec[3]
	self.question = spec[4]
	self.var = Tkinter.StringVar()

    def __repr__ (self):
	self.value = self.choices[self.var.get()]
	rep = [ "enum", self.tweakable, self.default, self.choices, self.question ]

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

    def make_widget(self, window, rownum):
	l = Tkinter.Label(window, text="   "+self.question)
	l.grid(row=rownum, column=0, sticky="w")
	# in case the value isn't one of the legal choices...
	valstr = str(self.value)
	# but it ought to be, so find it
	for string, val in self.choices.iteritems():
	    if str(val) == str(self.value):
		valstr = string
		break
	self.var.set(valstr)
	if self.tweakable:
	    o = Tkinter.OptionMenu(window, self.var, *self.choices.keys())
	    o.grid(row=rownum, column=1, sticky="w")
	else:
	    l = Tkinter.Label(window, text=valstr)
	    l.grid(row=rownum, column=1, sticky="w")
	return rownum+1

class ConstantSymbol(BaseSymbol):
    def __init__ (self, name, value, spec):
	self.name = name
	self.value = value

    def __repr__ (self):
	rep = [ "constant", self.tweakable, self.default ]

    def report (self):
	return str(self.value)

    def make_widget(self, window, rownum):
	print "Constant: "+self.name+" = "+str(self.value)
	return rownum


class PinSymbol(BaseSymbol):
    def __init__ (self, name, value, spec):
	self.name = name
	self.value = value
	self.tweakable = spec[1]
	self.default = spec[2]
	self.description = spec[3]

    def __repr__ (self):
	rep = [ "pin", self.tweakable, self.default, self.description ]

    def report (self):
	return self.description+": pin "+str(self.value)

    def make_widget(self, window, rownum):
	print "PinSpec: "+self.name+" = "+str(self.value)
	return rownum



class FpgaFile:
    def __init__ (self, name, window):
	print "FpgaFile.__init__("+name+")"
	self.changed = 0
	self.bitfile = bitfile.BitFile.fromfilename(name)
	# get generic info from file
	self.info = eval(self.bitfile["i"])
	# get ram template from file
	self.ram_template = string.Template(eval(self.bitfile["t"]))

	# get symbol specs from file
	symbol_specs = eval(self.bitfile["s"])
	
	#for name, spec_def in eval(self.bitfile["s"]).iteritems():
	#    self.symbol_specs[name] = SymbolSpec(spec_def)

	# get symbol values from file
	symbol_dict = eval(self.bitfile["v"])
	print repr(symbol_dict)
	self.symbol_table = []
	for name, value in symbol_dict.iteritems():
	    # try to find a matching spec
	    # first check for exact match
	    if name in symbol_specs.keys():
		spec = symbol_specs[name]
	    else:
		# remove instance number and try again
		pieces = name.split("__")
		sname = pieces[0]+"__"+pieces[2]
		if sname in symbol_specs.keys():
		    spec = symbol_specs[sname]
		else:
		    spec = ["constant", 0, 0]
	    print "name is: "+name+" spec is: "+repr(spec)	
	
	    vartype = spec[0]
	    if vartype == "constant":
		symbol = ConstantSymbol(name, value, spec)
	    elif vartype == "pinnumber":
		symbol =  PinSymbol(name, value, spec)
	    elif vartype == "pin":
		symbol =  PinSymbol(name, value, spec)
	    elif vartype == "bool":
		symbol = BoolSymbol(name, value, spec)
	    elif vartype == "enum":
		symbol = EnumSymbol(name, value, spec)
	    else:
		raise ValueError, "unknown type: %r"%vartype
		
	    self.symbol_table.append(symbol)

	self.symbol_table.sort()
	# create tabbed notebook for pin and module data
	self.notebook = bwidget.NoteBook(app, arcradius=2);
	self.notebook.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)
	
	# create module info tab
	self.module_page = self.notebook.insert(Tkinter.END, 0, text="Modules");
	self.module_area = bwidget.ScrolledWindow(self.module_page, auto=Tkinter.BOTH);
	self.module_area.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)
	self.module_scrollframe = bwidget.ScrollableFrame(self.module_area, constrainedwidth=1)
	self.module_area.setwidget(self.module_scrollframe)
	self.module_frame = self.module_scrollframe.getframe()
	self.module_frame.configure()
	self.module_frame.grid_columnconfigure(1, weight=1)
	# populate it with data
	rownum = 0
	instance = ""
	for symbol in sorted(self.symbol_table):
	    if symbol.get_modtype() == "pinnimber":
		continue
	    if symbol.get_instname() != instance:
		instance = symbol.get_instname()
		l = Tkinter.Label(self.module_frame, text=instance)
		l.grid(row=rownum, sticky="w")
		rownum += 1
	    rownum = symbol.make_widget(self.module_frame, rownum)
	self.notebook.raise_page(0)
	
	# create pin info tab
	self.pin_page = self.notebook.insert(Tkinter.END, 1, text="Pinout");
	self.pin_area = bwidget.ScrolledWindow(self.pin_page, auto=Tkinter.BOTH);
	self.pin_area.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)
	self.pin_scrollframe = bwidget.ScrollableFrame(self.pin_area, constrainedwidth=1)
	self.pin_area.setwidget(self.pin_scrollframe)
	self.pin_frame = self.pin_scrollframe.getframe()
	self.pin_frame.configure()
	self.pin_frame.grid_columnconfigure(1, weight=1)
	# populate it with data
	rownum = 0
	instance = ""
	for symbol in sorted(self.symbol_table):
	    if symbol.get_modtype() != "pinnumber":
		print "skipping module ", symbol.get_instname()
		continue
	    if symbol.get_instname() != instance:
		print "creating pin ", symbol.get_instname
		instance = symbol.get_instname()
		l = Tkinter.Label(self.pin_frame, text=instance)
		l.grid(row=rownum, sticky="w")
		rownum += 1
	    rownum = symbol.make_widget(self.pin_frame, rownum)

	print "  .__init__() done"

    def save(self, name):
	print "FpgaFile.save("+name+")"
	# 'v' section is symbol values which might have changed
	# re-build it from the symbol table
	symbol_dict = {}
	for symbol in self.symbol_table:
	    symbol_dict[symbol.name] = symbol.value
	self.bitfile["v"] = repr(symbol_dict)
	# make 'r' section is RAM data, create it using template
	# and latest symbol values
	# perform text substitution
	text = self.ram_template.substitute(symbol_dict)
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
	self.bitfile["r"] = bytes
	self.bitfile.tofilename(name)
	self.changed = 0
	print "  .save() done"

    def close(self):
	print "FpgaFile.close()"
	if self.changed == 1:
	    if tkMessageBox.askyesno("File Changed","Save changes?") :
		self.save()
	print "  .close(): clean up"
	# do whatever is needed here to clear out things
	self.work_area.destroy()
	self.name = ""
	self.changed = 0
	disable_buttons()
	app.title('FPGA Configuration Editor')
	print "  .close() done"
	return 0

    def make_report(self):
	#global report_pins
	print "FpgaFile.make_report()"
	print "  pins: ", tk_report_pins.get(), "  modules: ", tk_report_modules.get()
	if tk_report_modules.get():
	    print "Module report:"
	    instance = ""
	    for symbol in sorted(self.symbol_table):
		if symbol.get_instname() != instance:
		    instance = symbol.get_instname()
		    print instance+":"
		rep = symbol.report()
		if ( not isinstance(symbol, ConstantSymbol) and
		     not isinstance(symbol, PinSymbol) ):
		    print "  "+rep
	if tk_report_pins.get():
	    print "Pin report:"
	    instance = ""
	    for symbol in sorted(self.symbol_table):
		if symbol.get_instname() != instance:
		    instance = symbol.get_instname()
		    print instance+":"
		rep = symbol.report()
		if ( isinstance(symbol, PinSymbol) ):
		    print "  "+rep

    def __repr__ (self):
	return repr([self.ram_template.template, self.symbol_table, self.symbol_specs])

    def get_orig_name (self):
	return self.info["orig_name"]

    def is_changed (self):
	return self.changed

    def touch(self):
	print "FpgaFile.touch()"
	self.changed = 1
	print "  .touch() done"


def report():
    if file_opened:
	fpga.make_report()

def touch():
    if file_opened:
	fpga.touch()

def file_open():
    global fpga, file_name, file_opened, file_rename_before_save
    print "file_open()"
    if file_opened:
	file_close()
    if len(file_name) == 0:
	file_name=tkFileDialog.askopenfilename(filetypes=[("FPGA configurations", "*.fpga"), ("All files", "*")])
    if len(file_name) == 0 :
	# user hit cancel (or something similar)
	return
    fpga = FpgaFile(file_name, app)
    file_opened = 1
    if fpga.get_orig_name() == os.path.basename(file_name):
	file_rename_before_save = 1
    else:
	file_rename_before_save = 0
    enable_buttons()
    app.title(os.path.basename(file_name))
    print "  file_open() done"

def file_save():
    print "file_save()"
    if not file_opened:
	return
    if file_rename_before_save:
	file_save_as()
    else:
	fpga.save(file_name)
    print "  file_save() done"

def file_save_as():
    global file_name, file_rename_before_save
    print "file_save_as()"
    if not file_opened:
	return
    name=tkFileDialog.asksaveasfilename(filetypes=[("FPGA configurations", "*.fpga")], defaultextension=".fpga")
    if len(name) == 0:
	# user hit cancel (or something similar)
	return
    file_name = name
    file_rename_before_save = 0
    file_save()
    app.title(os.path.basename(file_name))
    print "  file_save_as() done"

def file_close():
    global fpga, file_name, file_opened, file_rename_before_save
    print "file_close()"
    if not file_opened:
	return
    if fpga.is_changed():
	if tkMessageBox.askyesno("File Changed","Save changes?") :
	    fpga.save(file_name)
    print "  file_close(): clean up"
    # do whatever is needed here to clear out things
    fpga.close()
    file_name = ""
    file_rename_before_save = 0
    fpga = None
    disable_buttons()
    app.title('FPGA Configuration Editor')
    file_opened = 0
    print "  file_close() done"


def disable_buttons ():
    file_menu.entryconfig("Save", state=Tkinter.DISABLED)
    file_menu.entryconfig("Save as...", state=Tkinter.DISABLED)
    file_menu.entryconfig("Close", state=Tkinter.DISABLED)
    report_menu.entryconfig("Create", state=Tkinter.DISABLED)

def enable_buttons ():
    file_menu.entryconfig("Save", state=Tkinter.NORMAL)
    file_menu.entryconfig("Save as...", state=Tkinter.NORMAL)
    file_menu.entryconfig("Close", state=Tkinter.NORMAL)
    report_menu.entryconfig("Create", state=Tkinter.NORMAL)


def make_menus(window):
    global top_menu, file_menu, report_menu
    top_menu = Tkinter.Menu(window)
    window.config(menu=top_menu)

    file_menu = Tkinter.Menu(top_menu, tearoff=0)
    file_menu.add_command(label='Open...', underline=0, command=file_open)
    file_menu.add_command(label='Save', underline=0, command=file_save, state=Tkinter.DISABLED)
    file_menu.add_command(label='Save as...', underline=0, command=file_save_as, state=Tkinter.DISABLED)
    file_menu.add_command(label='Close', underline=0, command=file_close, state=Tkinter.DISABLED)
    file_menu.add('separator')
    file_menu.add_command(label='Touch', underline=0, command=touch)
    file_menu.add_command(label='Quit', underline=0, command=top_menu.quit)
    top_menu.add_cascade(label='File', menu=file_menu, underline=0)

    report_menu = Tkinter.Menu(top_menu, tearoff=0)
    report_menu.add_checkbutton(label='Pins', variable=tk_report_pins)
    report_menu.add_checkbutton(label='Modules', variable=tk_report_modules)
    report_menu.add('separator')
    report_menu.add_command(label='Create', underline=0, command=report, state=Tkinter.DISABLED)
    top_menu.add_cascade(label='Report', menu=report_menu, underline=0)

app = Tkinter.Tk()
app.title('FPGA Configuration Editor')
app.pack_propagate(0);
app.configure(width=800, height=600)

tk_report_pins = Tkinter.IntVar()
tk_report_pins.set(1)
tk_report_modules = Tkinter.IntVar()
tk_report_modules.set(1)

make_menus(app)

file_opened = 0
file_rename_before_save = 0
file_name = ""

# this is the persistent main work window, with a scrollable frame inside
# it will persist over any number of open-close cycles
#work_area = bwidget.ScrolledWindow(app, auto=Tkinter.BOTH);
#work_area.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)
#contents = bwidget.ScrollableFrame(work_area, constrainedwidth=1)
#work_area.setwidget(contents)
#frame = contents.getframe()
#frame.configure(bg="white")

# the inner frame is going to be created and packed full of widgets
# when the user opens a file, and destroyed when he closes the file
#innerframe = Tkinter.Frame(frame)
#innerframe.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)
#innerframe.grid_columnconfigure(1, weight=1)

#vars = []
#l = Tkinter.Label(frame, text="Item Name", rel="raised", bd=2)
#l.grid(row=0, column=0, sticky="ew")
#l = Tkinter.Label(frame, text="Value", rel="raised", bd=2, justify="right")
#l.grid(row=0, column=1, sticky="ew")
#for i in range(20):
#    l = Tkinter.Label(frame, text="Item %d" % i, bg="white")
#    l.grid(row=i+1, column=0, sticky="e")
#    v = Tkinter.IntVar(app); v.set(i*i); vars.append(v)
#    e = Tkinter.Entry(frame, textv=v)
#    e.grid(row=i+1, column=1, sticky="ew")


if len(sys.argv) == 2 :
    file_name = sys.argv[1]
    file_open()
app.mainloop()

print "reached exit"

