#!/usr/bin/env python2
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

class SymbolSpec:
    def __init__ (self, deflist) :
	#print "SymbolSpec.__init__("+repr(deflist)+")"
	self.vartype = deflist[0]
	self.tweakable = deflist[1]
	self.default = deflist[2]
	if self.vartype == "pin":
	    self.description = deflist[3]
	if self.vartype == "enum":
	    self.choices = deflist[3]
	    self.question = deflist[4]
	if self.vartype == "bool":
	    self.question = deflist[3]

    def to_list (self):
	rep = [ self.vartype, self.tweakable, self.default ]
	if self.vartype == "constant":
	    return rep
	if self.vartype == "pin":
	    rep.append(self.description)
	    return rep
	if self.vartype == "enum":
	    rep.append(self.choices)
	    rep.append(self.question)
	    return rep
	if self.vartype == "bool":
	    rep.append(self.question)
	    return rep

    def __repr__ (self):
	return repr(self.to_list())

class Symbol:
    def __init__ (self, name, value, specs):
	#print "Symbol.__init__("+name+","+repr(value)+")"
	self.name = name
	self.value = value
	# try to find a matching spec
	# first check for exact match
	if name in specs.keys():
	    self.spec = specs[name]
	else:
	    # remove instance number and try again
	    pieces = name.split("__")
	    name = pieces[0]+"__"+pieces[2]
	    if name in fpga.symbol_specs.keys():
		self.spec = specs[name]
	    else:
		self.spec = [ "constant", 0, value ]

    def __repr__ (self):
	rep = (self.name, self.value, self.spec)
	return repr(rep)

    def __cmp__ (self, other):
	if self.name > other.name:
	   return 1
	if self.name < other.name:
	   return -1
	return 0


class FpgaFile:
    def __init__ (self):
	print "FpgaFile.__init__()"
	self.name = ""
	self.opened = 0
	self.changed = 0
	self.rename_before_save = 0
	print "  .__init__() done"

    def open_dialog(self):
	print "FpgaFile.open_dialog()"
	fname=tkFileDialog.askopenfilename(filetypes=[("FPGA configurations", "*.fpga"), ("All files", "*")])
	if len(fname) != 0 :
	    self.open(fname)
	print "  .opendialog() done"

    def open(self, name):
	print "FpgaFile.open("+name+")"
	if self.opened :
	    self.close()
	self.name = name
	self.bitfile = bitfile.BitFile.fromfilename(name)
	# get generic info from file
	self.info = eval(self.bitfile["i"])
	# get ram template from file
	self.ram_template = string.Template(eval(self.bitfile["t"]))
	# get symbol specs from file
	self.symbol_specs = {}
	for name, spec_def in eval(self.bitfile["s"]).iteritems():
	    self.symbol_specs[name] = SymbolSpec(spec_def)
	# get symbol values from file
	symbol_dict = eval(self.bitfile["v"])
	self.symbol_table = []
	for name, value in symbol_dict.iteritems():
	    self.symbol_table.append(Symbol(name, value, self.symbol_specs))
	self.symbol_table.sort()
	self.opened = 1
	if self.info["orig_name"] == os.path.basename(self.name):
	    self.rename_before_save = 1
	enable_buttons()
	app.title(os.path.basename(self.name))
	print "  .open() done"

    def save_dialog(self):
	print "FpgaFile.save_dialog()"
	fname=tkFileDialog.asksaveasfilename(filetypes=[("FPGA configurations", "*.fpga")], defaultextension=".fpga")
	if len(fname) != 0 :
	    self.rename(fname)
	    self.save()
	print "  .save_dialog() done"

    def save(self):
	print "FpgaFile.save()"
	if self.rename_before_save:
	    self.save_dialog()
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
	self.bitfile.tofilename(self.name)
	self.changed = 0
	print "  .save() done"

    def rename(self, name):
	print "FpgaFile.rename("+name+")"
	self.name = name
	app.title(os.path.basename(name))
	self.changed = 1
	self.rename_before_save = 0
	print "  .rename() done"

    def close(self):
	print "FpgaFile.close() on '"+self.name+"'"
	if self.opened == 0:
	    return 0
	if self.changed == 1:
	    if tkMessageBox.askyesno("File Changed","Save changes?") :
		self.save()
	print "  .close(): clean up"
	# do whatever is needed here to clear out things
	self.name = ""
	self.opened = 0
	self.changed = 0
	self.rename_before_save = 0
	disable_buttons()
	app.title('FPGA Configuration Editor')
	print "  .close() done"
	return 0

    def make_report(self):
	global report_pins
	print "FpgaFile.make_report()"
	print "  pins: ", tk_report_pins.get(), "  modules: ", tk_report_modules.get()
	for symbol in self.symbol_table:
	    print symbol

    def __str__ (self):
	return repr([self.name, self.opened, self.changed, self.renamed])

    def __repr__ (self):
	return repr([self.name, self.ram_template.template, self.symbol_table, self.symbol_specs])

    def touch(self):
	print "FpgaFile.touch()"
	self.changed = 1
	print "  .touch() done"


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
    file_menu.add_command(label='Open...', underline=0, command=fpga.open_dialog)
    file_menu.add_command(label='Save', underline=0, command=fpga.save, state=Tkinter.DISABLED)
    file_menu.add_command(label='Save as...', underline=0, command=fpga.save_dialog, state=Tkinter.DISABLED)
    file_menu.add_command(label='Close', underline=0, command=fpga.close, state=Tkinter.DISABLED)
    file_menu.add('separator')
    file_menu.add_command(label='Touch', underline=0, command=fpga.touch)
    file_menu.add_command(label='Quit', underline=0, command=top_menu.quit)
    top_menu.add_cascade(label='File', menu=file_menu, underline=0)

    report_menu = Tkinter.Menu(top_menu, tearoff=0)
    report_menu.add_checkbutton(label='Pins', variable=tk_report_pins)
    report_menu.add_checkbutton(label='Modules', variable=tk_report_modules)
    report_menu.add('separator')
    report_menu.add_command(label='Create', underline=0, command=fpga.make_report, state=Tkinter.DISABLED)
    top_menu.add_cascade(label='Report', menu=report_menu, underline=0)

app = Tkinter.Tk()
app.title('FPGA Configuration Editor')
app.pack_propagate(0);
app.configure(width=800, height=600)

fpga = FpgaFile()
tk_report_pins = Tkinter.IntVar()
tk_report_pins.set(1)
tk_report_modules = Tkinter.IntVar()
tk_report_modules.set(1)

make_menus(app)

work_area = bwidget.ScrolledWindow(app, auto=Tkinter.BOTH);
work_area.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)

contents = bwidget.ScrollableFrame(work_area, constrainedwidth=1)
work_area.setwidget(contents)
frame = contents.getframe()
frame.configure(bg="white")
frame.grid_columnconfigure(1, weight=1)

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
    fpga.open(sys.argv[1])
app.mainloop()

print "reached exit"

