#!/usr/bin/env python2
#    This is a component of AXIS, a front-end for LinuxCNC
#    Copyright 2006 Jeff Epler <jepler@unpythonic.net> and
#    Chris Radek <chris@timeguy.com>
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

"""\
axis-remote: trigger commands in a running AXIS GUI

Usage: axis-remote --clear|--ping|--reload|--quit|--mdi command|filename
       axis-remote -c|-p|-r|-q|-m command|filename"""

import sys, getopt, Tkinter, os

UNSPECIFIED, OPEN, RELOAD, PING, CLEAR, MDI, QUIT = range(7)
mode = UNSPECIFIED

def usage(exitval=0):
    print __doc__
    raise SystemExit, exitval

try:
    opts, args = getopt.getopt(sys.argv[1:], "h?prqcm",
                        ['help','ping', 'reload', 'quit', 'clear', 'mdi'])
except getopt.GetoptError, detail:
    print detail
    usage(99)

for o, a in opts:
    if o in ('-h', '-?', '--help'):
        usage(0)
    elif o in ('-c', '--clear'):
        if mode != UNSPECIFIED:
            usage(99)
        mode = CLEAR
    elif o in ('-p', '--ping'):
        if mode != UNSPECIFIED:
            usage(99)
        mode = PING
    elif o in ('-r', '--reload'):
        if mode != UNSPECIFIED:
            usage(99)
        mode = RELOAD
    elif o in ('-q', '--quit'):
        if mode != UNSPECIFIED:
            usage(99)
        mode = QUIT
    elif o in ('-m', '--mdi'):
        if mode != UNSPECIFIED:
            usage(99)
        mode = MDI
if mode == UNSPECIFIED:
    mode = OPEN

if mode == OPEN:
    if len(args) != 1:
        usage(99)
elif mode == MDI:
    if len(args) != 1:
        usage(99)
else:
    if len(args) != 0:
        usage(99)

t = Tkinter.Tk(); t.wm_withdraw()

msg = ""
try:
    if mode == PING:
        try:
            t.tk.call("send", "axis", "expr", "1")
        except Tkinter.TclError, detail:
            raise SystemExit, 1
    # cmds below are checked for suitability by axis remote() function
    #      return "" if ok
    elif mode == OPEN:
        msg = t.tk.call("send", "axis", ("remote","open_file_name", os.path.abspath(args[0])))
    elif mode == MDI:
        msg = t.tk.call("send", "axis", ("remote","send_mdi_command", args[0]))
    elif mode == RELOAD:
        msg = t.tk.call("send", "axis", ("remote","reload_file"))
    elif mode == CLEAR:
        msg = t.tk.call("send", "axis", ("remote","clear_live_plot"))
    elif mode == QUIT:
        msg = t.tk.call("send", "axis", ("remote","destroy"))
except Tkinter.TclError,detail:
    raise SystemExit,detail

if msg:
    raise SystemExit,msg
# vim:sw=4:sts=4:et:
