#!/usr/bin/python
"""Usage:
    python teach.py nmlfile outputfile
If outputfile is not specified, writes to standard output.

You must ". scripts/rip-environment" before running this script, if you use
run-in-place.
"""
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
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

import linuxcnc
import Tkinter
import sys

linenumber = 1;

if len(sys.argv) > 1:
    linuxcnc.nmlfile = sys.argv[1]

if len(sys.argv) > 2:
    outfile = sys.argv[2]
    sys.stdout = open(outfile, 'w')

s = linuxcnc.stat()

def get_cart():
    s.poll()
    position = " ".join(["%-8.4f"] * s.axes)
    return position % s.position[:s.axes]
    
def get_joint():
    s.poll()
    position = " ".join(["%-8.4f"] * s.axes)
    return position % s.joint_actual_position[:s.axes]

def log():
    global linenumber;
    if world.get():
	p = get_cart()
    else:
	p = get_joint()
    label1.configure(text='Learned:  %s' % p)
    print linenumber, p, s.flood, s.mist, s.lube, s.spindle_enabled;
    linenumber += 1;

def show():
    s.poll()
    if world.get():
	p = get_cart()
    else:
	p = get_joint()
    label2.configure(text='Position: %s' % p)
    app.after(100, show)

app = Tkinter.Tk(); app.wm_title('Machinekit Teach-In')

world = Tkinter.IntVar(app)

button = Tkinter.Button(app, command=log, text='Learn', font=("helvetica", 14))
button.pack(side='left')

label2 = Tkinter.Label(app, width=60, font='fixed', anchor="w")
label2.pack(side='top')

label1 = Tkinter.Label(app, width=60, font='fixed', text="Learned:  (nothing yet)", anchor="w")
label1.pack(side='top')

r1 = Tkinter.Radiobutton(app, text="Joint", variable=world, value=0)
r1.pack(side='left')
r2 = Tkinter.Radiobutton(app, text="World", variable=world, value=1)
r2.pack(side='left')

show()
app.mainloop()
