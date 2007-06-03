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

# this is copied over from spec2fpga, and will need changes to
# make it do GUI stuff
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
		self.default = 0
		self.description = definition["description"]
	    elif self.vartype == "enum":
		self.default = definition["default"]
		self.choices = definition["options"]
		self.question = definition["question"]
	    elif self.vartype == "bool":
		self.default = definition["default"]
		self.question = definition["question"]
	    else:
		print "ERROR: symbol spec '"+defstr+"' has illegal type '"+var_type+"'"
		sys.exit(1)
	except KeyError, key:
	    print "ERROR: symbol spec '"+defstr+"' is missing item '"+str(key)+"'"
	    sys.exit(1)
	except KeyValueError, (key, value):
	    print "ERROR: symbol spec '"+defstr+"' has bad value '"+value+"' for item '"+str(key)+"'"
	    sys.exit(1)

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
	global app
	print "FpgaFile.open("+name+")"
	if self.opened :
	    self.close()
	self.name = name
	self.bitfile = bitfile.BitFile.fromfilename(name)
	# get generic info from file
	self.info = eval(self.bitfile["i"])
	# get ram template from file
	self.ram_template = string.Template(eval(self.bitfile["t"]))
	# get symbol values from file
	self.symbol_table = eval(self.bitfile["v"])
	# get symbol specs from file
	self.symbol_specs = eval(self.bitfile["s"])
	self.opened = 1
	if self.info["orig_name"] == os.path.basename(name):
	    self.rename_before_save = 1
	app.title(os.path.basename(name))
	file_menu.entryconfig(2, state=Tkinter.NORMAL)
	file_menu.entryconfig(3, state=Tkinter.NORMAL)
	file_menu.entryconfig(4, state=Tkinter.NORMAL)
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
	# 'v' section - symbol values - might have changed
	self.bitfile["v"] = repr(self.symbol_table)
	# make 'r' section - RAM data - using new symbols
	# perform text substitution
	text = self.ram_template.substitute(self.symbol_table)
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
	global app
	print "FpgaFile.rename("+name+")"
	self.name = name
	app.title(os.path.basename(name))
	self.changed = 1
	self.rename_before_save = 0
	print "  .rename() done"

    def close(self):
	global app
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
	file_menu.entryconfig(2, state=Tkinter.DISABLED)
	file_menu.entryconfig(3, state=Tkinter.DISABLED)
	file_menu.entryconfig(4, state=Tkinter.DISABLED)
	app.title('FPGA Configuration Editor')
	print "  .close() done"
	return 0

    def __str__ (self):
	return repr([self.name, self.opened, self.changed, self.renamed])

    def __repr__ (self):
	return repr([self.name, self.ram_template.template, self.symbol_table, self.symbol_specs])

    def touch(self):
	print "FpgaFile.touch()"
	self.changed = 1
	print "  .touch() done"

def make_menus(window):
    global top_menu, file_menu, fpga
    top_menu = Tkinter.Menu(window)
    window.config(menu=top_menu)

    file_menu = Tkinter.Menu(top_menu)
    file_menu.add_command(label='Open...', underline=0, command=fpga.open_dialog)
    file_menu.add_command(label='Save', underline=0, command=fpga.save, state=Tkinter.DISABLED)
    file_menu.add_command(label='Save as...', underline=0, command=fpga.save_dialog, state=Tkinter.DISABLED)
    file_menu.add_command(label='Close', underline=0, command=fpga.close, state=Tkinter.DISABLED)
    file_menu.add('separator')
    file_menu.add_command(label='Touch', underline=0, command=fpga.touch)
    file_menu.add_command(label='Quit', underline=0, command=top_menu.quit)
    top_menu.add_cascade(label='File', menu=file_menu, underline=0)



app = Tkinter.Tk()
app.title('FPGA Configuration Editor')
app.grid_propagate(0);
app.configure(width=800, height=600)

fpga = FpgaFile()
make_menus(app)
work_area = bwidget.ScrolledWindow(app, auto=Tkinter.BOTH);
work_area.pack(expand=Tkinter.YES, fill=Tkinter.BOTH)

contents = bwidget.ScrollableFrame(work_area, constrainedwidth=1)
work_area.setwidget(contents)
frame = contents.getframe()
frame.configure(bg="white")
frame.grid_columnconfigure(1, weight=1)

fpga = None
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


#if len(sys.argv) == 2 :
#    open_file(sys.argv[1])
app.mainloop()

print "reached exit"
sys.exit(1)

