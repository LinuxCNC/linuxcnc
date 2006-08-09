#!/usr/bin/env python2
#    This is a component of AXIS, a front-end for emc
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

"""\
axis-remote: cause AXIS to open, reload its opened file, or exit

Usage: axis-remote [--ping|--reload|--quit|filename]
       axis-remote [-p|-r|-q]"""

import sys, getopt, Tkinter, os

OPEN, RELOAD, PING, QUIT = range(4)
mode = OPEN

def usage(exitval=0):
    print __doc__
    raise SystemExit, exitval

try:
    opts, args = getopt.getopt(sys.argv[1:], "h?prq",
                        ['help','ping', 'reload', 'quit'])
except getopt.GetoptError, detail:
    print detail
    usage(99)

for o, a in opts:
    if o in ('-h', '-?', '--help'):
        usage(0)
    elif o in ('-p', '--ping'):
        if mode != OPEN:
            usage(99)
        mode = PING
    elif o in ('-r', '--reload'):
        if mode != OPEN:
            usage(99)
        mode = RELOAD
    elif o in ('-q', '--quit'):
        if mode != OPEN:
            usage(99)
        mode = QUIT

if mode == OPEN:
    if len(args) != 1:
        usage(99)
else:
    if len(args) != 0:
        usage(99)

t = Tkinter.Tk(); t.wm_withdraw()

if mode == OPEN:
    t.tk.call("send", "axis", "open_file_name", os.path.abspath(args[0]))
elif mode == PING:
    try:
        t.tk.call("send", "axis", "expr", "1")
    except Tkinter.TclError, detail:
        raise SystemExit, 1
    raise SystemExit, 0
elif mode == RELOAD:
    t.tk.call("send", "axis", "reload_file")
elif mode == QUIT:
    t.tk.call("send", "axis", "destroy", ".")

# vim:sw=4:sts=4:et:
