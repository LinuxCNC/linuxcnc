#!/usr/bin/env python2
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import sys, os
import linuxcnc, time
import rs274.options

import gettext
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
gettext.install("linuxcnc", localedir=os.path.join(BASE, "share", "locale"), unicode=True)

if len(sys.argv) > 1 and sys.argv[1] == '-ini':
    ini = linuxcnc.ini(sys.argv[2])
    linuxcnc.nmlfile = ini.find("EMC", "NML_FILE") or linuxcnc.nmlfile
    del sys.argv[1:3]

s = linuxcnc.stat(); s.poll()

def show_mcodes(l):
    return " ".join(["M%g" % i for i in l[1:] if i != -1])
def show_gcodes(l):
    return " ".join(["G%g" % (i/10.) for i in l[1:] if i != -1])
position = " ".join(["%-8.4f"] * s.axes)
def show_position(p):
    return position % p[:s.axes]
peraxis = " ".join(["%s"] * s.axes)
def show_peraxis(p):
    return peraxis % p[:s.axes]
def show_float(p): return "%-8.4f" % p

maps = {
'exec_state': {linuxcnc.EXEC_ERROR: 'error',
                linuxcnc.EXEC_DONE: 'done',
                linuxcnc.EXEC_WAITING_FOR_MOTION: 'motion',
                linuxcnc.EXEC_WAITING_FOR_MOTION_QUEUE: 'motion queue',
                linuxcnc.EXEC_WAITING_FOR_IO: 'io',
                linuxcnc.EXEC_WAITING_FOR_MOTION_AND_IO: 'motion and io',
                linuxcnc.EXEC_WAITING_FOR_DELAY: 'delay',
                linuxcnc.EXEC_WAITING_FOR_SYSTEM_CMD: 'system command',
                linuxcnc.EXEC_WAITING_FOR_SPINDLE_ORIENTED: 'spindle orient'},
'motion_mode':{linuxcnc.TRAJ_MODE_FREE: 'free', linuxcnc.TRAJ_MODE_COORD: 'coord',
                linuxcnc.TRAJ_MODE_TELEOP: 'teleop'},
'interp_state':{linuxcnc.INTERP_IDLE: 'idle', linuxcnc.INTERP_PAUSED: 'paused', 
                linuxcnc.INTERP_READING: 'reading', linuxcnc.INTERP_WAITING: 'waiting'},
'task_state':  {linuxcnc.STATE_ESTOP: 'estop', linuxcnc.STATE_ESTOP_RESET: 'estop reset',
                linuxcnc.STATE_ON: 'on', linuxcnc.STATE_OFF: 'off'},
'task_mode':   {linuxcnc.MODE_AUTO: 'auto', linuxcnc.MODE_MDI: 'mdi',
                linuxcnc.MODE_MANUAL: 'manual'},
'state':       {1: 'rcs_done', 2: 'rcs_exec', 3: 'rcs_error'},
'motion_type': {0: 'none', 1: 'traverse', 2: 'feed', 3: 'arc', 4: 'toolchange', 5: 'probing'},
'program_units': {1: 'inch', 2: 'mm'},
'kinematics_type': {linuxcnc.KINEMATICS_IDENTITY: 'identity', linuxcnc.KINEMATICS_FORWARD_ONLY: 'forward_only', 
                    linuxcnc.KINEMATICS_INVERSE_ONLY: 'inverse_only', linuxcnc.KINEMATICS_BOTH: 'both'},
'mcodes': show_mcodes, 'gcodes': show_gcodes, 'poll': None, 'tool_table': None,
'axis': None, 'gettaskfile': None,
'actual_position': show_position, 
'position': show_position, 
'dtg': show_position, 
'joint_position': show_position,
'joint_actual_position': show_position,
'origin': show_position,
'rotation_xy': show_float,
'probed_position': show_position,
'tool_offset': show_position,
'g5x_offset': show_position,
'g92_offset': show_position,
'limit': show_peraxis,
'homed': show_peraxis,
'linear_units': show_float,
'max_acceleration': show_float,
'max_velocity': show_float,
'angular_units': show_float,
'distance_to_go': show_float,
'current_vel': show_float,
}

if s.kinematics_type == 1:
    maps['joint_position'] = None
    maps['joint_actual_position'] = None

def gui():
    import Tkinter
    from _tkinter import TclError
    root = Tkinter.Tk(className="LinuxCNCTop")
    rs274.options.install(root)
    root.title(_("Machinekit Status"))

    t = Tkinter.Text()
    sb = Tkinter.Scrollbar(command=t.yview)
    t.configure(yscrollcommand=sb.set)
    t.configure(tabs="150")

    base_font = t.tk.call("set", "BASE_FONT")
    fixed_font = t.tk.call("set", "FIXED_FONT")
    t.tag_configure("key", foreground="blue", font=base_font)
    t.tag_configure("value", foreground="black", font=fixed_font)
    t.tag_configure("changedvalue", foreground="black", background="red", font=fixed_font)
    t.tag_configure("sel", foreground="white")
    t.tag_raise("sel")
    t.bind("<KeyPress>", "break")

    b = Tkinter.Button(text=_("Copy All"),
        command="%s tag add sel 0.0 end; tk_textCopy %s" % (t, t))
    b.pack(side="bottom", anchor="sw")

    t.pack(side="left", expand=1, fill="both")
    sb.pack(side="left", expand=0, fill="y")

    changetime = {}
    oldvalues = {}
    def timer():
        try:
            s.poll()
        except linuxcnc.error:
            root.destroy()
        pos = t.yview()[0]
        selection = t.tag_ranges("sel")
        insert_point = t.index("insert")
        insert_gravity = t.mark_gravity("insert")
        try:
            anchor_point = t.index("anchor")
            anchor_gravity = t.mark_gravity("anchor")
        except TclError:
            anchor_point = None
        t.delete("0.0", "end")
        first = True
        for k in dir(s):
            if k.startswith("_"): continue
            if maps.has_key(k) and maps[k] == None: continue
            v = getattr(s, k)
            if maps.has_key(k):
                m = maps[k]
                if callable(m):
                    v = m(v)
                else:
                    v = m.get(v, v)
            if oldvalues.has_key(k):
                changed = oldvalues[k] != v
                if changed: changetime[k] = time.time() + 2
            oldvalues[k] = v
            if changetime.has_key(k) and changetime[k] >= time.time():
                vtag = "changedvalue"
            else:
                vtag = "value"
            if first: first = False
            else: t.insert("end", "\n")
            t.insert("end", k, "key", "\t")
            t.insert("end", v, vtag)
        t.yview_moveto(pos)
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
    s.poll()
    for k in dir(s):
        if k.startswith("_"): continue
        if maps.has_key(k) and maps[k] == None: continue
        v = getattr(s, k)
        if maps.has_key(k):
            m = maps[k]
            if callable(m):
                v = m(v)
            else:
                v = m.get(v, v)
        print "%-20s %-.58s" % (k, v)

if len(sys.argv) > 1 and sys.argv[1] == '-t':
    text()
else:
    gui()


# vim:sw=4:sts=4:et
