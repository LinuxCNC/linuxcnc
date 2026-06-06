#!/usr/bin/env python3
#    This is a component of AXIS, a front-end for linuxcnc
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
#                         and Chris Radek <chris@timeguy.com>
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

import sys, os
import gmi
from gmi.constants import *
import time
import rs274.options

import gettext
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
gettext.install("linuxcnc", localedir=os.path.join(BASE, "share", "locale"))

if len(sys.argv) > 1 and sys.argv[1] == '-ini':
    del sys.argv[1:3]

s = gmi.Stat()

def show_spindles(l):
    ct = 0; s = ""
    for d in l:
        for key in d:
            s = s+"%d %20s %s\n"% (ct,key,d[key])
        ct = ct+1
    return s

def show_mcodes(l):
    return " ".join(["M%g" % i for i in l[1:] if i != -1])
    
def show_gcodes(l):
    return " ".join(["G%g" % (i/10.) for i in l[1:] if i != -1])
    
def show_position(p):
    return " ".join(["%-8.4f" % n for i, n in enumerate(p) if s.axis_mask & (1<<i)])

joint_position = None
def show_joint_position(p):
    global joint_position
    if joint_position is None:
        joint_position = " ".join(["%-8.4f"] * s.joints) if s.joints else ""
    return joint_position % p[:s.joints] if s.joints else ""

perjoint = None
def show_perjoint(p):
    global perjoint
    if perjoint is None:
        perjoint = " ".join(["%s"] * s.joints) if s.joints else ""
    return perjoint % p[:s.joints] if s.joints else ""
    
def show_float(p): return "%-8.4f" % p

def show_floats(s): return " ".join(show_float(p) for p in s)
def show_ints(s): return " ".join(str(bool(p)) for p in s)

maps = {
'exec_state': {EXEC_ERROR: 'error',
                EXEC_DONE: 'done',
                EXEC_WAITING_FOR_MOTION: 'motion',
                EXEC_WAITING_FOR_MOTION_QUEUE: 'motion queue',
                EXEC_WAITING_FOR_IO: 'io',
                EXEC_WAITING_FOR_MOTION_AND_IO: 'motion and io',
                EXEC_WAITING_FOR_DELAY: 'delay',
                EXEC_WAITING_FOR_MCODE_HANDLER: 'M-code handler',
                EXEC_WAITING_FOR_SPINDLE_ORIENTED: 'spindle orient'},
'motion_mode':{TRAJ_MODE_FREE: 'free', TRAJ_MODE_COORD: 'coord',
                TRAJ_MODE_TELEOP: 'teleop'},
'interp_state':{INTERP_IDLE: 'idle', INTERP_PAUSED: 'paused', 
                INTERP_READING: 'reading', INTERP_WAITING: 'waiting'},
'task_state':  {STATE_ESTOP: 'estop', STATE_ESTOP_RESET: 'estop reset',
                STATE_ON: 'on', STATE_OFF: 'off'},
'task_mode':   {MODE_AUTO: 'auto', MODE_MDI: 'mdi',
                MODE_MANUAL: 'manual'},
'state':       {1: 'rcs_done', 2: 'rcs_exec', 3: 'rcs_error'},
'motion_type': {0: 'none', 1: 'traverse', 2: 'feed', 3: 'arc', 4: 'toolchange', 5: 'probing'},
'program_units': {1: 'inch', 2: 'mm'},
'kinematics_type': {KINEMATICS_IDENTITY: 'identity', KINEMATICS_FORWARD_ONLY: 'forward_only', 
                    KINEMATICS_INVERSE_ONLY: 'inverse_only', KINEMATICS_BOTH: 'both'},
'mcodes': show_mcodes, 'gcodes': show_gcodes, 'poll': None, 'tool_table': None,
'spindle':show_spindles,
'axis': None, 'joint': None, 'gettaskfile': None,
'actual_position': show_position, 
'position': show_position, 
'dtg': show_position, 
'origin': show_position,
'rotation_xy': show_float,
'probed_position': show_position,
'tool_offset': show_position,
'g5x_offset': show_position,
'g92_offset': show_position,
'linear_units': show_float,
'max_acceleration': show_float,
'max_velocity': show_float,
'angular_units': show_float,
'distance_to_go': show_float,
'current_vel': show_float,
'ain': show_floats,
'aout': show_floats,
'din': show_ints,
'dout': show_ints,
'settings': show_floats,
'limit': show_perjoint,
'homed': show_perjoint,
'joint_position': show_joint_position,
'joint_actual_position': show_joint_position,
}

_maps_initialized = False
def _init_maps():
    global _maps_initialized
    if _maps_initialized:
        return
    _maps_initialized = True
    if s.kinematics_type == 1:
        maps['joint_position'] = None
        maps['joint_actual_position'] = None

def gui():
    import tkinter

    from _tkinter import TclError
    root = tkinter.Tk(className="LinuxCNCTop")
    rs274.options.install(root)
    root.title(_("LinuxCNC Status"))

    t = tkinter.Text(wrap="word")
    sb = tkinter.Scrollbar(command=t.yview)
    t.configure(yscrollcommand=sb.set)
    t.configure(tabs=150)

    base_font = t.tk.call("set", "BASE_FONT")
    fixed_font = t.tk.call("set", "FIXED_FONT")
    t.tag_configure("key", foreground="blue", font=base_font)
    t.tag_configure("value", foreground="black", font=fixed_font, lmargin1=150, lmargin2=150)
    t.tag_configure("changedvalue", foreground="black", background="red", font=fixed_font, lmargin1=150, lmargin2=150)
    t.tag_configure("sel", foreground="white")
    t.tag_raise("sel")
    t.bind("<KeyPress>", "break")

    b = tkinter.Button(text=_("Copy All"),
        command="%s tag add sel 0.0 end; tk_textCopy %s" % (t, t))
    b.pack(side="bottom", anchor="sw")

    t.pack(side="left", expand=1, fill="both")
    sb.pack(side="left", expand=0, fill="y")

    changetime = {}
    oldvalues = {}
    def timer():
        _init_maps()
        try:
            s.poll()
        except Exception:
            root.destroy()
        selection = t.tag_ranges("sel")
        insert_point = t.index("insert")
        insert_gravity = t.mark_gravity("insert")
        try:
            anchor_point = t.index("anchor")
            anchor_gravity = t.mark_gravity("anchor")
        except TclError:
            anchor_point = None
        first = True
        now = time.time()
        for k in dir(s):
            if k.startswith("_"): continue
            if k in maps and maps[k] == None: continue
            v = getattr(s, k)
            if k in maps:
                m = maps[k]
                if callable(m):
                    v = m(v)
                else:
                    v = m.get(v, v)
            v = str(v)
            v = v.strip()
            v = v or "-"
            changed = oldvalues.get(k, None) != v
            if changed: changetime[k] = time.time() + 2
            oldvalues[k] = v
            if changed:
                vtag = "changedvalue"
            elif k in changetime and changetime[k] < now:
                vtag = "value"
                if k in changetime: del changetime[k]
            elif not changed:
                continue
            vranges = t.tag_ranges(k)
            if vranges:
                t.tk.call(t, "replace", "%s.first" % k, "%s.last" % k, v, (k, vtag))
            else:
                if first: first = False
                else: t.insert("end", "\n")
                t.insert("end", k, "key", "\t")
                t.insert("end", v, (k, vtag))
        if selection:
            t.tag_add("sel", *selection)
        t.mark_set("insert", insert_point)
        t.mark_gravity("insert", insert_gravity)
        if anchor_point is not None:
            t.mark_set("anchor", anchor_point)
            t.mark_gravity("anchor", anchor_gravity)
        t.after(100, timer)
    timer()
    t.mainloop()

def text():
    _init_maps()
    s.poll()
    for k in dir(s):
        if k.startswith("_"): continue
        if k in maps and maps[k] == None: continue
        v = getattr(s, k)
        if k in maps:
            m = maps[k]
            if callable(m):
                v = m(v)
            else:
                v = m.get(v, v)
        print("%-20s %-.58s" % (k, v))

if len(sys.argv) > 1 and sys.argv[1] == '-t':
    text()
else:
    gui()


# vim:sw=4:sts=4:et
