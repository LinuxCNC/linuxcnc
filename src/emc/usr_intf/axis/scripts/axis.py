#!/usr/bin/env python2
#    This is a component of AXIS, a front-end for emc
#    Copyright 2004, 2005, 2006, 2007, 2008, 2009
#    Jeff Epler <jepler@unpythonic.net> and Chris Radek <chris@timeguy.com>
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


# import pdb

import sys, os
import string
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

# otherwise, on hardy the user is shown spurious "[application] closed
# unexpectedly" messages but denied the ability to actually "report [the]
# problem"
sys.excepthook = sys.__excepthook__

import gettext;
gettext.install("emc2", localedir=os.path.join(BASE, "share", "locale"), unicode=True)

import array, time, atexit, tempfile, shutil, errno, thread, select, re

# Print Tk errors to stdout. python.org/sf/639266
import Tkinter 
OldTk = Tkinter.Tk
class Tk(OldTk):
    def __init__(self, *args, **kw):
        OldTk.__init__(self, *args, **kw)
        self.tk.createcommand('tkerror', self.tkerror)

    def tkerror(self, arg):
        print "TCL error in asynchronous code:"
        print self.tk.call("set", "errorInfo")

Tkinter.Tk = Tk

from Tkinter import *
from minigl import *
from rs274.OpenGLTk import *
from rs274.interpret import StatMixin
from rs274.glcanon import GLCanon
from hershey import Hershey
from propertywindow import properties
import rs274.options
import nf
import gcode
import locale
import bwidget
from math import hypot, atan2, sin, cos, pi, sqrt
from rs274 import ArcsToSegmentsMixin
import emc

if os.environ.has_key("AXIS_NO_HAL"):
    hal_present = 0;
else:
    hal_present = 1;

if hal_present == 1 :
    import hal


import ConfigParser
cp = ConfigParser.ConfigParser
class AxisPreferences(cp):
    types = {
        bool: cp.getboolean,
        float: cp.getfloat,
        int: cp.getint,
        str: cp.get,
        repr: lambda self,section,option: eval(cp.get(self,section,option)),
    }

    def __init__(self):
        cp.__init__(self)
        self.fn = os.path.expanduser("~/.axis_preferences")
        self.read(self.fn)

    def getpref(self, option, default=False, type=bool):
        m = self.types.get(type)
        try:
            o = m(self, "DEFAULT", option)
        except Exception, detail:
            print detail
            self.set("DEFAULT", option, default)
            self.write(open(self.fn, "w"))
            o = default
        return o

    def putpref(self, option, value, type=bool):
        self.set("DEFAULT", option, type(value))
        self.write(open(self.fn, "w"))

ap = AxisPreferences()

root_window = Tkinter.Tk(className="Axis")
nf.start(root_window)
nf.makecommand(root_window, "_", _)
rs274.options.install(root_window)
root_window.tk.call("set", "version", emc.version)

try:
    nf.source_lib_tcl(root_window,"axis.tcl")
except TclError:
    print root_window.tk.call("set", "errorInfo")
    raise
program_start_line = 0
program_start_line_last = -1

lathe = 0
mdi_history_max_entries = 1000
mdi_history_save_filename= "~/.axis_mdi_history"



feedrate_blackout = 0
spindlerate_blackout = 0
maxvel_blackout = 0
jogincr_index_last = 1
mdi_history_index= -1

homeicon = array.array('B', 
        [0x2, 0x00,   0x02, 0x00,   0x02, 0x00,   0x0f, 0x80,
        0x1e, 0x40,   0x3e, 0x20,   0x3e, 0x20,   0x3e, 0x20,
        0xff, 0xf8,   0x23, 0xe0,   0x23, 0xe0,   0x23, 0xe0,
        0x13, 0xc0,   0x0f, 0x80,   0x02, 0x00,   0x02, 0x00])

limiticon = array.array('B',
        [  0,   0,  128, 0,  134, 0,  140, 0,  152, 0,  176, 0,  255, 255,
         255, 255,  176, 0,  152, 0,  140, 0,  134, 0,  128, 0,    0,   0,
           0,   0,    0, 0])

help1 = [
    ("F1", _("Emergency stop")),
    ("F2", _("Turn machine on")),
    ("", ""),
    ("X, `", _("Activate first axis")),
    ("Y, 1", _("Activate second axis")),
    ("Z, 2", _("Activate third axis")),
    ("A, 3", _("Activate fourth axis")),
    ("4..8", _("Activate fifth through ninth axis")),
    ("`, 1..9, 0", _("Set Feed Override from 0% to 100%")),
    (_(", and ."), _("Select jog speed")),
    (_("< and >"), _("Select angular jog speed")),
    (_("I, Shift-I"), _("Select jog increment")),
    ("C", _("Continuous jog")),
    (_("Home"), _("Send active axis home")),
    (_("Ctrl-Home"), _("Home all axes")),
    (_("Shift-Home"), _("Zero G54 offset for active axis")),
    (_("End"), _("Set G54 offset for active axis")),
    ("-, =", _("Jog active axis")),

    ("", ""),
    (_("Left, Right"), _("Jog first axis")),
    (_("Up, Down"), _("Jog second axis")),
    (_("Pg Up, Pg Dn"), _("Jog third axis")),
    (_("Shift+above jogs"), _("Jog at traverse speed")),
    ("[, ]", _("Jog fourth axis")),

    ("", ""),
    ("D", _("Toggle between Drag and Rotate mode")),
    (_("Left Button"), _("Pan, rotate or select line")),
    (_("Shift+Left Button"), _("Rotate or pan")),
    (_("Right Button"), _("Zoom view")),
    (_("Wheel Button"), _("Rotate view")),
    (_("Rotate Wheel"), _("Zoom view")),
    (_("Control+Left Button"), _("Zoom view")),
]
help2 = [
    ("F3", _("Manual control")),
    ("F5", _("Code entry (MDI)")),
    (_("Control-M"), _("Clear MDI history")),
    (_("Control-H"), _("Copy selected MDI history elements")),
    ("",          _("to clipboard")),
    (_("Control-Shift-H"), _("Paste clipboard to MDI history")),
    ("L", _("Override Limits")),
    ("", ""),
    ("O", _("Open program")),
    (_("Control-R"), _("Reload program")),
    (_("Control-S"), _("Save g-code as")),
    ("R", _("Run program")),
    ("T", _("Step program")),
    ("P", _("Pause program")),
    ("S", _("Resume program")),
    ("ESC", _("Stop running program, or")),
    ("", _("stop loading program preview")),
    ("", ""),
    ("F7", _("Toggle mist")),
    ("F8", _("Toggle flood")),
    ("B", _("Spindle brake off")),
    (_("Shift-B"), _("Spindle brake on")),
    ("F9", _("Turn spindle clockwise")),
    ("F10", _("Turn spindle counterclockwise")),
    ("F11", _("Turn spindle more slowly")),
    ("F12", _("Turn spindle more quickly")),
    ("", ""),
    (_("Control-K"), _("Clear live plot")),
    ("V", _("Cycle among preset views")),
    ("F4", _("Switch between preview and DRO")),
    ("", ""),
    (_("Ctrl-Space"), _("Clear notifications")),
]


def install_help(app):
    keys = nf.makewidget(app, Frame, '.keys.text')
    for i in range(len(help1)):
        a, b = help1[i]
        Label(keys, text=a, font="fixed", padx=4, pady=0, highlightthickness=0).grid(row=i, column=0, sticky="w")
        Label(keys, text=b, padx=4, pady=0, highlightthickness=0).grid(row=i, column=1, sticky="w")
    for i in range(len(help2)):
        a, b = help2[i]
        Label(keys, text=a, font="fixed", padx=4, pady=0, highlightthickness=0).grid(row=i, column=3, sticky="w")
        Label(keys, text=b, padx=4, pady=0, highlightthickness=0).grid(row=i, column=4, sticky="w")
    Label(keys, text="    ").grid(row=0, column=2)

color_names = [
    ('back', 'Background'),
    'dwell', 'm1xx', 'straight_feed', 'arc_feed', 'traverse',
    'backplotjog', 'backplotfeed', 'backplotarc', 'backplottraverse',
    'backplottoolchange', 'backplotprobing',
    'selected',

    'tool_ambient', 'tool_diffuse', 'lathetool',

    'overlay_foreground', ('overlay_background', 'Background'),

    'label_ok', 'label_limit',

    'small_origin', 'axis_x', 'axis_y', 'axis_z',
    'cone',
]   

def joints_mode():
    return s.motion_mode == emc.TRAJ_MODE_FREE and s.kinematics_type != emc.KINEMATICS_IDENTITY

def parse_color(c):
    if c == "": return (1,0,0)
    return tuple([i/65535. for i in root_window.winfo_rgb(c)])

def to_internal_units(pos, unit=None):
    if unit is None:
        unit = s.linear_units
    lu = (unit or 1) * 25.4

    lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
    return [a/b for a, b in zip(pos, lus)]

def to_internal_linear_unit(v, unit=None):
    if unit is None:
        unit = s.linear_units
    lu = (unit or 1) * 25.4
    return v/lu

def from_internal_units(pos, unit=None):
    if unit is None:
        unit = s.linear_units
    lu = (unit or 1) * 25.4

    lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
    return [a*b for a, b in zip(pos, lus)]

def from_internal_linear_unit(v, unit=None):
    if unit is None:
        unit = s.linear_units
    lu = (unit or 1) * 25.4
    return v*lu

class Notification(Tkinter.Frame):
    def __init__(self, master):
        self.widgets = []
        self.cache = []
        Tkinter.Frame.__init__(self, master)

    def clear(self,iconname=None):
        if iconname:
            cpy = self.widgets[:]
            for i, item in enumerate(cpy):
                frame,icon,text,button,iname = item
                if iname == "icon_std_" + iconname:
                    self.remove(cpy[i])
        else:
            while self.widgets:
                self.remove(self.widgets[0])

    def clear_one(self):
        if self.widgets:
            self.remove(self.widgets[0])


    def add(self, iconname, message):
        self.place(relx=1, rely=1, y=-20, anchor="se")
        iconname = self.tk.call("load_image", "std_" + iconname)
        close = self.tk.call("load_image", "close", "notification-close")
        if len(self.widgets) > 10:
            self.remove(self.widgets[0])
        if self.cache:
            frame, icon, text, button, discard = widgets = self.cache.pop()
            icon.configure(image=iconname)
            text.configure(text=message)
            widgets = frame, icon, text, button, iconname
        else:
            frame = Tkinter.Frame(self)
            icon = Tkinter.Label(frame, image=iconname)
            text = Tkinter.Label(frame, text=message, wraplength=300, justify="left")
            button = Tkinter.Button(frame, image=close,
                command=lambda: self.remove(widgets))
            widgets = frame, icon, text, button, iconname
            text.pack(side="left")
            icon.pack(side="left")
            button.pack(side="left")
        frame.pack(side="top", anchor="e")
        self.widgets.append(widgets)

    def remove(self, widgets):
        self.widgets.remove(widgets)
        if len(self.cache) < 10:
            widgets[0].pack_forget()
            self.cache.append(widgets)
        else:
            widgets[0].destroy()
        if len(self.widgets) == 0:
            self.place_forget()

def soft_limits():
    def fudge(x):
        if abs(x) > 1e30: return 0
        return x

    ax = s.joint
    return (
        to_internal_units([fudge(ax[i]['min_position_limit']) for i in range(3)]),
        to_internal_units([fudge(ax[i]['max_position_limit']) for i in range(3)]))

class MyOpengl(Opengl):
    def __init__(self, *args, **kw):
        self.after_id = None
        self.motion_after = None
        self.perspective = False
        Opengl.__init__(self, *args, **kw)
        self.bind('<Button-4>', self.zoomin)
        self.bind('<Button-5>', self.zoomout)
        self.bind('<MouseWheel>', self.zoomwheel)
        self.bind('<Button-1>', self.select_prime, add=True)
        self.bind('<ButtonRelease-1>', self.select_fire, add=True)
        self.bind('<Button1-Motion>', self.translate_or_rotate)
        self.bind('<Button1-Motion>', self.select_cancel, add=True)
        self.bind("<Control-Button-1>", self.start_zoom)
        self.bind("<Control-B1-Motion>", self.continue_zoom)
        self.bind("<Button-3>", self.start_zoom)
        self.bind("<B3-Motion>", self.continue_zoom)
        self.bind("<Shift-Button-1>", self.StartRotate)
        self.bind("<Shift-B1-Motion>", self.rotate_or_translate)
        self.bind("<B2-Motion>", self.rotate_or_translate)
        self.highlight_line = None
        self.select_event = None
        self.select_buffer_size = 100
        self.select_primed = False
        self.last_position = None
        self.last_homed = None
        self.last_origin = None
        self.last_tool = None
        self.last_limits = None
        self.g = None
        self.set_eyepoint(5.)
        self.get_resources()

    def translate_or_rotate(self, event):
        if vars.rotate_mode.get():
            self.tkRotate(event)
        else:
            self.tkTranslate(event)

    def rotate_or_translate(self, event):
        if not vars.rotate_mode.get():
            self.tkRotate(event)
        else:
            self.tkTranslate(event)

    def basic_lighting(self):
        self.activate()
        glLightfv(GL_LIGHT0, GL_POSITION, (1, -1, 1, 0))
        glLightfv(GL_LIGHT0, GL_AMBIENT, self.colors['tool_ambient'] + (0,))
        glLightfv(GL_LIGHT0, GL_DIFFUSE, self.colors['tool_diffuse'] + (0,))
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (1,1,1,0))
        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glDepthFunc(GL_LESS)
        glEnable(GL_DEPTH_TEST)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

    def start_zoom(self, event):
        self.y0 = event.y
        self.original_zoom = self.distance

    def continue_zoom(self, event):
        dy = event.y - self.y0
        self.distance = self.original_zoom * pow(1.25, dy / 16.)
        self.tkRedraw()

    def get_resources(self):
        self.colors = {}
        for c in color_names:
            if isinstance(c, tuple):
                c, d = c
            else:
                d = "Foreground"
            self.colors[c] = parse_color(self.option_get(c, d))
        self.colors['backplotjog_alpha'] = \
            float(self.option_get("backplotjog_alpha", "Float"))
        self.colors['backplotfeed_alpha'] = \
            float(self.option_get("backplotfeed_alpha", "Float"))
        self.colors['backplotarc_alpha'] = \
            float(self.option_get("backplotarc_alpha", "Float"))
        self.colors['backplottraverse_alpha'] = \
            float(self.option_get("backplottraverse_alpha", "Float"))
        self.colors['backplottoolchange_alpha'] = \
            float(self.option_get("backplottoolchange_alpha", "Float"))
        self.colors['backplotprobing_alpha'] = \
            float(self.option_get("backplotprobing_alpha", "Float"))
        self.colors['overlay_alpha'] = \
            float(self.option_get("overlay_alpha", "Float"))
        x = float(self.option_get("tool_light_x", "Float"))
        y = float(self.option_get("tool_light_y", "Float"))
        z = float(self.option_get("tool_light_z", "Float"))
        dist = (x**2 + y**2 + z**2) ** .5
        self.light_position = (x/dist, y/dist, z/dist, 0)
        self.colors['tool_alpha'] = \
            float(self.option_get("tool_alpha", "Float"))
        self.colors['lathetool_alpha'] = \
            float(self.option_get("lathetool_alpha", "Float"))

    def select_prime(self, event):
        self.select_primed = event

    def select_cancel(self, event):
        self.select_primed = False

    def select_fire(self, event):
        if self.select_primed: self.queue_select(event)

    def queue_select(self, event):
        self.select_event = event
        self.tkRedraw()

    def deselect(self, event):
        self.set_highlight_line(None)

    def select(self, event):
        if self.g is None: return
        pmatrix = glGetDoublev(GL_PROJECTION_MATRIX)
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        vport = glGetIntegerv(GL_VIEWPORT)
        gluPickMatrix(event.x, vport[3]-event.y, 5, 5, vport)
        glMultMatrixd(pmatrix)
        glMatrixMode(GL_MODELVIEW)

        while 1:
            glSelectBuffer(self.select_buffer_size)
            glRenderMode(GL_SELECT)
            glInitNames()
            glPushName(0)

            
            if not vars.show_rapids.get() and norapids is not None:
                glCallList(select_norapids)
            else:
                glCallList(select_program)

            try:
                buffer = list(glRenderMode(GL_RENDER))
            except GLerror, detail:
                if detail.errno[0] == GL_STACK_OVERFLOW:
                    self.select_buffer_size *= 2
                    continue
                raise
            break

        buffer.sort()

        if buffer:
            min_depth, max_depth, names = buffer[0]
            self.set_highlight_line(names[0])
        else:
            self.set_highlight_line(None)

        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)

    def set_current_line(self, line):
        if line == vars.running_line.get(): return
        t.tag_remove("executing", "0.0", "end")
        if line is not None and line > 0:
            vupdate(vars.running_line, line)
            if vars.highlight_line.get() <= 0:
                t.see("%d.0" % (line+2))
                t.see("%d.0" % line)
            t.tag_add("executing", "%d.0" % line, "%d.end" % line)
        else:
            vupdate(vars.running_line, 0)

    def set_highlight_line(self, line):
        if line == vars.highlight_line.get(): return
        self.highlight_line = line
        t.tag_remove("sel", "0.0", "end")
        if line is not None and line > 0:
            t.see("%d.0" % (line+2))
            t.see("%d.0" % line)
            t.tag_add("sel", "%d.0" % line, "%d.end" % line)
            vupdate(vars.highlight_line, line)
        else:
            vupdate(vars.highlight_line, -1)
        global highlight
        if highlight is not None: glDeleteLists(highlight, 1)
        highlight = glGenLists(1)
        glNewList(highlight, GL_COMPILE)
        if line is not None and self.g is not None:
            x, y, z = self.g.highlight(line, geometry)
            self.set_centerpoint(x, y, z)
        elif self.g is not None:
            x = (self.g.min_extents[0] + self.g.max_extents[0])/2
            y = (self.g.min_extents[1] + self.g.max_extents[1])/2
            z = (self.g.min_extents[2] + self.g.max_extents[2])/2
            self.set_centerpoint(x, y, z)
        glEndList()
 
    def zoomin(self, event):
        self.distance = self.distance / 1.25
        self.tkRedraw()

    def zoomout(self, event):
        self.distance = self.distance * 1.25
        self.tkRedraw()

    def zoomwheel(self, event):
        if event.delta > 0: self.zoomin(event)
        else: self.zoomout(event)

    def tkRedraw(self, *dummy):
        if self.after_id:
            # May need to upgrade to an instant redraw
            self.after_cancel(self.after_id)
        self.after_id = self.after_idle(self.actual_tkRedraw)

    def redraw_soon(self, *dummy):
        if self.after_id: return
        self.after_id = self.after(50, self.actual_tkRedraw)

    def tkRedraw_perspective(self, *dummy):
        """Cause the opengl widget to redraw itself."""

        if not self.initialised: return
        self.activate()

        glPushMatrix()                  # Protect our matrix
        self.update_idletasks()
        self.activate()
        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)

        # Clear the background and depth buffer.
        glClearColor(*(self.colors['back'] + (0,)))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(self.fovy, float(w)/float(h), self.near, self.far + self.distance)

        gluLookAt(0, 0, self.distance,
            0, 0, 0,
            0., 1., 0.)
        glMatrixMode(GL_MODELVIEW)

        # Call objects redraw method.
        self.redraw()
        glFlush()                               # Tidy up
        glPopMatrix()                   # Restore the matrix

        self.tk.call(self._w, 'swapbuffers')

    def tkRedraw_ortho(self, *dummy):
        """Cause the opengl widget to redraw itself."""

        if not self.initialised: return
        self.activate()

        glPushMatrix()                  # Protect our matrix
        self.update_idletasks()
        self.activate()
        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)

        # Clear the background and depth buffer.
        glClearColor(*(self.colors['back'] + (0,)))
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        ztran = self.distance
        k = sqrt(abs(ztran or 1))
        l = k * h / w
        glOrtho(-k, k, -l, l, -1000, 1000.)

        gluLookAt(0, 0, 1,
            0, 0, 0,
            0., 1., 0.)
        glMatrixMode(GL_MODELVIEW)

        # Call objects redraw method.
        self.redraw()
        glFlush()                               # Tidy up
        glPopMatrix()                   # Restore the matrix

        self.tk.call(self._w, 'swapbuffers')

    def startRotate(self, event):
        if lathe: return
        return Opengl.startRotate(self, event)

    def tkAutoSpin(self, event):
        if lathe: return
        return Opengl.tkAutoSpin(self, event)

    def tkRotate(self, event):
        if lathe: return
        Opengl.tkRotate(self, event)
        self.perspective = True
        widgets.view_z.configure(relief="link")
        widgets.view_z2.configure(relief="link")
        widgets.view_x.configure(relief="link")
        widgets.view_y.configure(relief="link")
        widgets.view_p.configure(relief="link")
        vars.view_type.set(0)

    def actual_tkRedraw(self, *dummy):
        self.after_id = None
        if self.perspective:
            self.tkRedraw_perspective()
        else:
            self.tkRedraw_ortho()

    def set_eyepoint_from_extents(self, e1, e2):
        w = self.winfo_width()
        h = self.winfo_height()

        ztran = max(2.0, e1, e2 * w/h) ** 2
        self.set_eyepoint(ztran - self.zcenter)


    def redraw(self):
        s.poll()

        if not self.winfo_viewable():
            return self.redraw_dro()

        machine_limit_min, machine_limit_max = soft_limits()

        if self.select_event:
            self.select(self.select_event)
            self.select_event = None

        glDisable(GL_LIGHTING)
        glMatrixMode(GL_MODELVIEW)

        if vars.show_program.get() and program is not None:
            if not vars.show_rapids.get() and norapids is not None:
                glCallList(norapids)
            else:
                glCallList(program)
            if highlight is not None: glCallList(highlight)

            if vars.show_extents.get():
                # Dimensions
                x,y,z,p = 0,1,2,3
                if str(widgets.view_x['relief']) == "sunken":
                    view = x
                elif str(widgets.view_y['relief']) == "sunken":
                    view = y
                elif (str(widgets.view_z['relief']) == "sunken" or
                      str(widgets.view_z2['relief']) == "sunken"):
                    view = z
                else:
                    view = p

                g = self.g

                dimscale = vars.metric.get() and 25.4 or 1.0
                fmt = vars.metric.get() and "%.1f" or "%.2f"

                pullback = max(g.max_extents[x] - g.min_extents[x],
                               g.max_extents[y] - g.min_extents[y],
                               g.max_extents[z] - g.min_extents[z],
                               2 ) * .1

                dashwidth = pullback/4
                charsize = dashwidth * 1.5
                halfchar = charsize * .5

                if view == z or view == p:
                    z_pos = g.min_extents[z]
                    zdashwidth = 0
                else:
                    z_pos = g.min_extents[z] - pullback
                    zdashwidth = dashwidth
                # x dimension

                color_limit(0)
                glBegin(GL_LINES)
                if view != x and g.max_extents[x] > g.min_extents[x]:
                    y_pos = g.min_extents[y] - pullback;
                    glVertex3f(g.min_extents[x], y_pos, z_pos)
                    glVertex3f(g.max_extents[x], y_pos, z_pos)

                    glVertex3f(g.min_extents[x], y_pos - dashwidth, z_pos - zdashwidth)
                    glVertex3f(g.min_extents[x], y_pos + dashwidth, z_pos + zdashwidth)

                    glVertex3f(g.max_extents[x], y_pos - dashwidth, z_pos - zdashwidth)
                    glVertex3f(g.max_extents[x], y_pos + dashwidth, z_pos + zdashwidth)

                # y dimension
                if view != y and g.max_extents[y] > g.min_extents[y]:
                    x_pos = g.min_extents[x] - pullback;
                    glVertex3f(x_pos, g.min_extents[y], z_pos)
                    glVertex3f(x_pos, g.max_extents[y], z_pos)

                    glVertex3f(x_pos - dashwidth, g.min_extents[y], z_pos - zdashwidth)
                    glVertex3f(x_pos + dashwidth, g.min_extents[y], z_pos + zdashwidth)

                    glVertex3f(x_pos - dashwidth, g.max_extents[y], z_pos - zdashwidth)
                    glVertex3f(x_pos + dashwidth, g.max_extents[y], z_pos + zdashwidth)

                # z dimension
                if view != z and g.max_extents[z] > g.min_extents[z]:
                    x_pos = g.min_extents[x] - pullback;
                    y_pos = g.min_extents[y] - pullback;
                    glVertex3f(x_pos, y_pos, g.min_extents[z]);
                    glVertex3f(x_pos, y_pos, g.max_extents[z]);

                    glVertex3f(x_pos - dashwidth, y_pos - zdashwidth, g.min_extents[z])
                    glVertex3f(x_pos + dashwidth, y_pos + zdashwidth, g.min_extents[z])

                    glVertex3f(x_pos - dashwidth, y_pos - zdashwidth, g.max_extents[z])
                    glVertex3f(x_pos + dashwidth, y_pos + zdashwidth, g.max_extents[z])

                glEnd()

                # Labels
                if vars.coord_type.get():
                    offset = to_internal_units(s.origin)
                else:
                    offset = 0, 0, 0
                if view != z and g.max_extents[z] > g.min_extents[z]:
                    if view == x:
                        x_pos = g.min_extents[x] - pullback;
                        y_pos = g.min_extents[y] - 6.0*dashwidth;
                    else:
                        x_pos = g.min_extents[x] - 6.0*dashwidth;
                        y_pos = g.min_extents[y] - pullback;

                    bbox = color_limit(g.min_extents[z] < machine_limit_min[z])
                    glPushMatrix()
                    f = fmt % ((g.min_extents[z]-offset[z]) * dimscale)
                    glTranslatef(x_pos, y_pos, g.min_extents[z] - halfchar)
                    glScalef(charsize, charsize, charsize)
                    glRotatef(-90, 0, 1, 0)
                    glRotatef(-90, 0, 0, 1)
                    if view != x:
                        glRotatef(-90, 0, 1, 0)
                    hershey.plot_string(f, 0, bbox)
                    glPopMatrix()

                    bbox = color_limit(g.max_extents[z] > machine_limit_max[z])
                    glPushMatrix()
                    f = fmt % ((g.max_extents[z]-offset[z]) * dimscale)
                    glTranslatef(x_pos, y_pos, g.max_extents[z] - halfchar)
                    glScalef(charsize, charsize, charsize)
                    glRotatef(-90, 0, 1, 0)
                    glRotatef(-90, 0, 0, 1)
                    if view != x:
                        glRotatef(-90, 0, 1, 0)
                    hershey.plot_string(f, 0, bbox)
                    glPopMatrix()

                    color_limit(0)
                    glPushMatrix()
                    f = fmt % ((g.max_extents[z] - g.min_extents[z]) * dimscale)
                    glTranslatef(x_pos, y_pos, (g.max_extents[z] + g.min_extents[z])/2)
                    glScalef(charsize, charsize, charsize)
                    if view != x:
                        glRotatef(-90, 0, 0, 1)
                    glRotatef(-90, 0, 1, 0)
                    hershey.plot_string(f, .5, bbox)
                    glPopMatrix()

                if view != y and g.max_extents[y] > g.min_extents[y]:
                    x_pos = g.min_extents[x] - 6.0*dashwidth;

                    bbox = color_limit(g.min_extents[y] < machine_limit_min[y])
                    glPushMatrix()
                    f = fmt % ((g.min_extents[y] - offset[y]) * dimscale)
                    glTranslatef(x_pos, g.min_extents[y] + halfchar, z_pos)
                    glRotatef(-90, 0, 0, 1)
                    glRotatef(-90, 0, 0, 1)
                    if view == x:
                        glRotatef(90, 0, 1, 0)
                        glTranslatef(dashwidth*1.5, 0, 0)
                    glScalef(charsize, charsize, charsize)
                    hershey.plot_string(f, 0, bbox)
                    glPopMatrix()

                    bbox = color_limit(g.max_extents[y] > machine_limit_max[y])
                    glPushMatrix()
                    f = fmt % ((g.max_extents[y] - offset[y]) * dimscale)
                    glTranslatef(x_pos, g.max_extents[y] + halfchar, z_pos)
                    glRotatef(-90, 0, 0, 1)
                    glRotatef(-90, 0, 0, 1)
                    if view == x:
                        glRotatef(90, 0, 1, 0)
                        glTranslatef(dashwidth*1.5, 0, 0)
                    glScalef(charsize, charsize, charsize)
                    hershey.plot_string(f, 0, bbox)
                    glPopMatrix()

                    color_limit(0)
                    glPushMatrix()
                    f = fmt % ((g.max_extents[y] - g.min_extents[y]) * dimscale)

                    glTranslatef(x_pos, (g.max_extents[y] + g.min_extents[y])/2,
                                z_pos)
                    glRotatef(-90, 0, 0, 1)
                    if view == x:
                        glRotatef(-90, 1, 0, 0)
                        glTranslatef(0, halfchar, 0)
                    glScalef(charsize, charsize, charsize)
                    hershey.plot_string(f, .5)
                    glPopMatrix()

                if view != x and g.max_extents[x] > g.min_extents[x]:
                    y_pos = g.min_extents[y] - 6.0*dashwidth;

                    bbox = color_limit(g.min_extents[x] < machine_limit_min[x])
                    glPushMatrix()
                    f = fmt % ((g.min_extents[x] - offset[x]) * dimscale)
                    glTranslatef(g.min_extents[x] - halfchar, y_pos, z_pos)
                    glRotatef(-90, 0, 0, 1)
                    if view == y:
                        glRotatef(90, 0, 1, 0)
                        glTranslatef(dashwidth*1.5, 0, 0)
                    glScalef(charsize, charsize, charsize)
                    hershey.plot_string(f, 0, bbox)
                    glPopMatrix()

                    bbox = color_limit(g.max_extents[x] > machine_limit_max[x])
                    glPushMatrix()
                    f = fmt % ((g.max_extents[x] - offset[x]) * dimscale)
                    glTranslatef(g.max_extents[x] - halfchar, y_pos, z_pos)
                    glRotatef(-90, 0, 0, 1)
                    if view == y:
                        glRotatef(90, 0, 1, 0)
                        glTranslatef(dashwidth*1.5, 0, 0)
                    glScalef(charsize, charsize, charsize)
                    hershey.plot_string(f, 0, bbox)
                    glPopMatrix()

                    color_limit(0)
                    glPushMatrix()
                    f = fmt % ((g.max_extents[x] - g.min_extents[x]) * dimscale)

                    glTranslatef((g.max_extents[x] + g.min_extents[x])/2, y_pos,
                                z_pos)
                    if view == y:
                        glRotatef(-90, 1, 0, 0)
                        glTranslatef(0, halfchar, 0)
                    glScalef(charsize, charsize, charsize)
                    hershey.plot_string(f, .5)
                    glPopMatrix()

        if vars.show_live_plot.get() or vars.show_program.get():
            glPushMatrix()

            if vars.coord_type.get() and (s.origin[0] or s.origin[1] or 
                                          s.origin[2]):
                draw_small_origin()
                origin = to_internal_units(s.origin)[:3]
                glTranslatef(*origin)
                draw_axes()
            else:
                draw_axes()
            glPopMatrix()

        if vars.show_machine_limits.get():
            glLineWidth(1)
            glColor3f(1.0,0.0,0.0)
            glLineStipple(1, 0x1111)
            glEnable(GL_LINE_STIPPLE)
            glBegin(GL_LINES)

            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_max[2])
            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_min[2])
            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_min[2])
            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_max[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_max[2])
            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_max[2])

            
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_min[2])

            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_min[2])

            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_max[2])

            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_max[2])


            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_min[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_min[2])

            glVertex3f(machine_limit_min[0], machine_limit_max[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_max[1], machine_limit_max[2])

            glVertex3f(machine_limit_min[0], machine_limit_min[1], machine_limit_max[2])
            glVertex3f(machine_limit_max[0], machine_limit_min[1], machine_limit_max[2])
            
            glEnd()
            glDisable(GL_LINE_STIPPLE)
            glLineStipple(2, 0xffff)
            
        if vars.show_live_plot.get():
            glDepthFunc(GL_LEQUAL)
            glLineWidth(3)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
            glEnable(GL_BLEND)
            glPushMatrix()
            lu = 1/((s.linear_units or 1)*25.4)
            glScalef(lu, lu, lu);
            glMatrixMode(GL_PROJECTION)
            glPushMatrix()
            glTranslatef(0,0,.003)

            live_plotter.logger.call()

            glDrawArrays(GL_LINE_STRIP, 0, o.live_plot_size)
            glPopMatrix()
            glMatrixMode(GL_MODELVIEW)
            glPopMatrix();
            glDisable(GL_BLEND)
            glLineWidth(1)
            glDepthFunc(GL_LESS);
            
        if live_plotter.running.get() and vars.show_tool.get():
            pos = live_plotter.logger.last(vars.show_live_plot.get())
            if pos is None: pos = [0] * 6
            if program is not None:
                g = self.g
                x,y,z = 0,1,2
                cone_scale = max(g.max_extents[x] - g.min_extents[x],
                               g.max_extents[y] - g.min_extents[y],
                               g.max_extents[z] - g.min_extents[z],
                               2 ) * .5
            else:
                cone_scale = 1
            rx, ry, rz = pos[3:6]
            pos = to_internal_units(pos[:3])
            glPushMatrix()
            glTranslatef(*pos)
            sign = 1
            for ch in geometry:
                if ch == '-':
                    sign = -1
                elif ch == 'A':
                    glRotatef(rx*sign, 1, 0, 0)
                    sign = 1
                elif ch == 'B':
                    glRotatef(ry*sign, 0, 1, 0)
                    sign = 1
                elif ch == 'C':
                    glRotatef(rz*sign, 0, 0, 1)
                    sign = 1
            glEnable(GL_BLEND)
            glEnable(GL_CULL_FACE);
            glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA);

            if lathe and current_tool and current_tool.orientation != 0:
                glBlendColor(0,0,0,o.colors['lathetool_alpha'])
                lathetool()
            else:
                glBlendColor(0,0,0,o.colors['tool_alpha'])
                if lathe:
                    glRotatef(90, 0, 1, 0)
                if current_tool and current_tool.diameter != 0:
                    dia = current_tool.diameter
                    r = to_internal_linear_unit(dia) / 2.
                    q = gluNewQuadric()
                    glEnable(GL_LIGHTING)
                    glColor3f(*o.colors['cone'])
                    gluCylinder(q, r, r, 8*r, 32, 1)
                    glPushMatrix()
                    glRotatef(180, 1, 0, 0)
                    gluDisk(q, 0, r, 32, 1)
                    glPopMatrix()
                    glTranslatef(0,0,8*r)
                    gluDisk(q, 0, r, 32, 1)
                    glDisable(GL_LIGHTING)
                    gluDeleteQuadric(q)
                else:    
                    glScalef(cone_scale, cone_scale, cone_scale)
                    glCallList(cone_program)
            glPopMatrix()

        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        ypos = self.winfo_height()
        glOrtho(0.0, self.winfo_width(), 0.0, ypos, -1.0, 1.0)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()

        limit, homed, posstrs, droposstrs = self.posstrs()

        maxlen = max([len(p) for p in posstrs])
        pixel_width = coordinate_charwidth * max(len(p) for p in posstrs)

        glDepthFunc(GL_ALWAYS)
        glDepthMask(GL_FALSE)
        glEnable(GL_BLEND)
        glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA)
        glColor3f(*o.colors['overlay_background'])
        glBlendColor(0,0,0,1-o.colors['overlay_alpha'])
        glBegin(GL_QUADS)
        glVertex3f(0, ypos, 1)
        glVertex3f(0, ypos - 8 - coordinate_linespace*len(posstrs), 1)
        glVertex3f(pixel_width+42, ypos - 8 - coordinate_linespace*len(posstrs), 1)
        glVertex3f(pixel_width+42, ypos, 1)
        glEnd()
        glDisable(GL_BLEND)

        maxlen = 0
        ypos -= coordinate_linespace+5
        i=0
        glColor3f(*o.colors['overlay_foreground'])
        for string in posstrs:
            maxlen = max(maxlen, len(string))
            glRasterPos2i(5, ypos)
            for char in string:
                glCallList(fontbase + ord(char))
            if i < len(homed) and homed[i]:
                glRasterPos2i(pixel_width + 8, ypos)
                glBitmap(13, 16, 0, 3, 17, 0, homeicon)
            if i < len(homed) and limit[i]:
                glBitmap(13, 16, 0, 1, 17, 0, limiticon)
            ypos -= coordinate_linespace
            i = i + 1
        glDepthFunc(GL_LESS)
        glDepthMask(GL_TRUE)

        glPopMatrix()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)


    def posstrs(self):
        if not joints_mode():
            if s.kinematics_type == emc.KINEMATICS_IDENTITY:
                homed = s.homed[:]
                limit = s.limit[:]
            else:
                homed = [0]*9
                limit = [0]*9

            if vars.display_type.get():
                positions = s.position
            else:
                positions = s.actual_position

            if vars.coord_type.get():
                positions = [(i-j) for i, j in zip(positions, s.tool_offset)]
                positions = [(i-j) for i, j in zip(positions, s.origin)]

                t = -s.rotation_xy
                t = math.radians(t)
                x = positions[0]
                y = positions[1]
                positions[0] = x * math.cos(t) - y * math.sin(t)
                positions[1] = x * math.sin(t) + y * math.cos(t)
            else:
                positions = list(positions)

            if a_axis_wrapped:
                positions[3] = math.fmod(positions[3], 360.0)
                if positions[3] < 0: positions[3] += 360.0

            if b_axis_wrapped:
                positions[4] = math.fmod(positions[4], 360.0)
                if positions[4] < 0: positions[4] += 360.0

            if c_axis_wrapped:
                positions[5] = math.fmod(positions[5], 360.0)
                if positions[5] < 0: positions[5] += 360.0

            positions = to_internal_units(positions)
            axisdtg = to_internal_units(s.dtg)

            if vars.metric.get():
                positions = from_internal_units(positions, 1)
                axisdtg = from_internal_units(axisdtg, 1)
                format = "%3s:% 9.3f"
                droformat = format + "  DTG%1s:% 9.3f"
            else:
                format = "%3s:% 9.4f"
                droformat = format + "  DTG%1s:% 9.4f"

            posstrs = [format % j for i, j in zip(range(9), zip("XYZABCUVW", positions)) if s.axis_mask & (1<<i)]
            droposstrs = [droformat % (j + k) for i, j, k in zip(range(9), zip("XYZABCUVW", positions), zip("XYZABCUVW", axisdtg)) if s.axis_mask & (1<<i)]

            if lathe:
                posstrs[0] = format % ("Rad", positions[0])
                posstrs.insert(1, format % ("Dia", positions[0]*2.0))
                droposstrs[0] = droformat % ("Rad", positions[0], "R", axisdtg[0])
                droposstrs.insert(1, format % ("Dia", positions[0]*2.0))

            if vars.show_machine_speed.get():
                spd = to_internal_linear_unit(s.current_vel)
                if vars.metric.get():
                    spd = spd * 25.4 * 60
                else:
                    spd = spd * 60
                posstrs.append(format % ("Vel", spd))

            if vars.show_distance_to_go.get():
                dtg = to_internal_linear_unit(s.distance_to_go)
                if vars.metric.get():
                    dtg *= 25.4
                posstrs.append(format % ("DTG", dtg))
        else:
            homed = s.homed[:]
            limit = s.limit[:]

            # N.B. no conversion here because joint positions are unitless
            posstrs = ["  %s:% 9.4f" % i for i in
                zip(range(num_joints), s.joint_actual_position)]
            droposstrs = posstrs

        return limit, homed, posstrs, droposstrs

    def redraw_dro(self):
        limit, homed, posstrs, droposstrs = self.posstrs()

        text = widgets.numbers_text

        font = "Courier 10 pitch"
        if not hasattr(self, 'font_width'):
            self.font_width = text.tk.call(
                "font", "measure", (font, -100, "bold"), "0")
            self.font_vertspace = text.tk.call(
                "font", "metrics", (font, -100, "bold"), "-linespace") - 100
            self.last_font = None
        font_width = self.font_width
        font_vertspace = self.font_vertspace

        text.delete("0.0", "end")
        t = droposstrs[:]
        i = 0
        for ts in t:
            if i < len(homed) and homed[i]:
                t[i] += "*"
            else:
                t[i] += " "
            if i < len(homed) and limit[i]:
                t[i] += "!" # !!!1!
            else:
                t[i] += " "
            i+=1
            
        text.insert("end", "\n".join(t))

        window_height = text.winfo_height()
        window_width = text.winfo_width()
        dro_lines = len(droposstrs)
        dro_width = len(droposstrs[0]) + 3
        # pixels of height required, for "100 pixel" font
        req_height = dro_lines * 100 + (dro_lines + 1) * font_vertspace
        # pixels of width required, for "100 pixel" font
        req_width = dro_width * font_width
        height_ratio = float(window_height) / req_height
        width_ratio = float(window_width) / req_width
        ratio = min(height_ratio, width_ratio)
        new_font = -int(100*ratio)
        if new_font != self.last_font:
            text.configure(font=(font, new_font, "bold"))
            self.last_font = new_font

def init():
    glDrawBuffer(GL_BACK)
    glDisable(GL_CULL_FACE)
    glLineStipple(2, 0x5555)
    glDisable(GL_LIGHTING)
    glClearColor(0,0,0,0)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1)

def draw_small_origin():
    r = 2.0/25.4
    glColor3f(*o.colors['small_origin'])

    glBegin(GL_LINE_STRIP)
    for i in range(37):
        theta = (i*10)*math.pi/180.0
        glVertex3f(r*cos(theta),r*sin(theta),0.0)
    glEnd()
    glBegin(GL_LINE_STRIP)
    for i in range(37):
        theta = (i*10)*math.pi/180.0
        glVertex3f(0.0, r*cos(theta), r*sin(theta))
    glEnd()
    glBegin(GL_LINE_STRIP)
    for i in range(37):
        theta = (i*10)*math.pi/180.0
        glVertex3f(r*cos(theta),0.0, r*sin(theta))
    glEnd()

    glBegin(GL_LINES);
    glVertex3f(-r, -r, 0.0)
    glVertex3f( r,  r, 0.0)
    glVertex3f(-r,  r, 0.0)
    glVertex3f( r, -r, 0.0)

    glVertex3f(-r, 0.0, -r)
    glVertex3f( r, 0.0,  r)
    glVertex3f(-r, 0.0,  r)
    glVertex3f( r, 0.0, -r)

    glVertex3f(0.0, -r, -r)
    glVertex3f(0.0,  r,  r)
    glVertex3f(0.0, -r,  r)
    glVertex3f(0.0,  r, -r)
    glEnd()

def draw_axes():
    x,y,z,p = 0,1,2,3
    if str(widgets.view_x['relief']) == "sunken":
        view = x
    elif str(widgets.view_y['relief']) == "sunken":
        view = y
    elif (str(widgets.view_z['relief']) == "sunken" or
          str(widgets.view_z2['relief']) == "sunken"):
        view = z
    else:
        view = p

    glPushMatrix()
    glRotatef(s.rotation_xy, 0, 0, 1)

    glColor3f(*o.colors['axis_x'])
    glBegin(GL_LINES);
    glVertex3f(1.0,0.0,0.0)
    glVertex3f(0.0,0.0,0.0)
    glEnd();

    if view != x:
        glPushMatrix()
        if lathe:
            glTranslatef(1.3, -0.1, 0)
            glTranslatef(0, 0, -0.1)
            glRotatef(-90, 0, 1, 0)
            glRotatef(90, 1, 0, 0)
            glTranslatef(0.1, 0, 0)
        else:
            glTranslatef(1.2, -0.1, 0)
            if view == y:
                glTranslatef(0, 0, -0.1)
                glRotatef(90, 1, 0, 0)
        glScalef(0.2, 0.2, 0.2)
        hershey.plot_string("X", 0.5)
        glPopMatrix()

    glColor3f(*o.colors['axis_y'])
    glBegin(GL_LINES);
    glVertex3f(0.0,0.0,0.0)
    glVertex3f(0.0,1.0,0.0)
    glEnd();

    if view != y:
        glPushMatrix()
        glTranslatef(0, 1.2, 0)
        if view == x:
            glTranslatef(0, 0, -0.1)
            glRotatef(90, 0, 1, 0)
            glRotatef(90, 0, 0, 1)
        glScalef(0.2, 0.2, 0.2)
        hershey.plot_string("Y", 0.5)
        glPopMatrix()

    glColor3f(*o.colors['axis_z'])
    glBegin(GL_LINES);
    glVertex3f(0.0,0.0,0.0)
    glVertex3f(0.0,0.0,1.0)
    glEnd();

    if view != z:
        glPushMatrix()
        glTranslatef(0, 0, 1.2)
        if lathe:
            glRotatef(-90, 0, 1, 0)
        if view == x:
            glRotatef(90, 0, 1, 0)
            glRotatef(90, 0, 0, 1)
        elif view == y or view == p:
            glRotatef(90, 1, 0, 0)
        if lathe:
            glTranslatef(0, -.1, 0)
        glScalef(0.2, 0.2, 0.2)
        hershey.plot_string("Z", 0.5)
        glPopMatrix()

    glPopMatrix()

def toggle_perspective(e):
    o.perspective = not o.perspective
    o.tkRedraw()

def select_line(event):
    i = t.index("@%d,%d" % (event.x, event.y))
    i = int(i.split('.')[0])
    o.set_highlight_line(i)
    o.tkRedraw()
    return "break"

def select_prev(event):
    if o.highlight_line is None:
        i = o.last_line
    else:
        i = max(1, o.highlight_line - 1)
    o.set_highlight_line(i)
    o.tkRedraw()

def select_next(event):
    if o.highlight_line is None:
        i = 1
    else:
        i = min(o.last_line, o.highlight_line + 1)
    o.set_highlight_line(i)
    o.tkRedraw()

def scroll_up(event):
    t.yview_scroll(-2, "units")

def scroll_down(event):
    t.yview_scroll(2, "units")

select_program = select_norapids = program = highlight = norapids = None

def make_cone():
    global cone_program
    cone_program = glGenLists(1)
    q = gluNewQuadric()
    glNewList(cone_program, GL_COMPILE)
    glEnable(GL_LIGHTING)
    glColor3f(*o.colors['cone'])
    gluCylinder(q, 0, .1, .25, 32, 1)
    glPushMatrix()
    glTranslatef(0,0,.25)
    gluDisk(q, 0, .1, 32, 1)
    glPopMatrix()
    glDisable(GL_LIGHTING)
    glEndList()
    gluDeleteQuadric(q)

current_tool = None
lathe_shapes = [
    None,                           # 0
    (1,-1), (1,1), (-1,1), (-1,-1), # 1..4
    (0,-1), (1,0), (0,1), (-1,0),   # 5..8
    (0,0)                           # 9
]

def lathetool():
    glDepthFunc(GL_ALWAYS)
    diameter, frontangle, backangle, orientation = current_tool[-4:]
    w = 3/8. 

    radius = to_internal_linear_unit(diameter) / 2.
    glColor3f(*o.colors['lathetool'])
    glBegin(GL_LINES)
    glVertex3f(-radius/2.0,0.0,0.0)
    glVertex3f(radius/2.0,0.0,0.0)
    glVertex3f(0.0,0.0,-radius/2.0)
    glVertex3f(0.0,0.0,radius/2.0)
    glEnd()

    glNormal3f(0,1,0)

    if orientation == 9:
        glBegin(GL_TRIANGLE_FAN)
        for i in range(37):
            t = i * math.pi / 18
            glVertex3f(radius * math.cos(t), 0.0, radius * math.sin(t))
        glEnd()
    else:
        dx, dy = lathe_shapes[orientation]

        min_angle = min(backangle, frontangle) * math.pi / 180
        max_angle = max(backangle, frontangle) * math.pi / 180

        sinmax = sin(max_angle)
        cosmax = cos(max_angle)
        tanmax = cos(max_angle)
        sinmin = sin(min_angle)
        cosmin = cos(min_angle)
        tanmin = cos(min_angle)

        circleminangle = - pi/2 + min_angle
        circlemaxangle = - 3*pi/2 + max_angle
        d0 = 0

        x1 = (w - d0)

        sz = max(w, 3*radius)

        glBegin(GL_TRIANGLE_FAN)
        glVertex3f(
            radius * dx + radius * math.sin(circleminangle) + sz * sinmin,
            0,
            radius * dy + radius * math.cos(circleminangle) + sz * cosmin)
        for i in range(37):
            #t = circleminangle + i * (circlemaxangle - circleminangle)/36.
            t = circleminangle + i * (circlemaxangle - circleminangle)/36.
            glVertex3f(radius*dx + radius * math.sin(t), 0.0, radius*dy + radius * math.cos(t))

        glVertex3f(
            radius * dx + radius * math.sin(circlemaxangle) + sz * sinmax,
            0,
            radius * dy + radius * math.cos(circlemaxangle) + sz * cosmax)

        glEnd()
    glDepthFunc(GL_LESS)

def make_selection_list(g):
    global select_program, select_norapids
    if select_program is None: select_program = glGenLists(1)
    if select_norapids is None: select_norapids = glGenLists(1)
    glNewList(select_program, GL_COMPILE)
    g.draw(1)
    glEndList()
    glNewList(select_norapids, GL_COMPILE)
    g.draw(1, False)
    glEndList()

def make_main_list(g):
    global program, norapids
    if program is None: program = glGenLists(1)
    glNewList(program, GL_COMPILE)
    g.draw(0, True)
    glEndList()

    if norapids is None: norapids = glGenLists(1)
    glNewList(norapids, GL_COMPILE)
    g.draw(0, False)
    glEndList()

import array

def vupdate(var, val):
    try:
        if var.get() == val: return
    except ValueError:
        pass
    var.set(val)

def colinear(p1, p2, p3):
    x, y, z = 0, 1, 2
    p = p2[x] - p1[x], p2[y] - p1[y], p2[z] - p1[z]      
    q = p3[x] - p2[x], p3[y] - p2[y], p3[z] - p2[z]     
    dp = sqrt(p[x]**2 + p[y]**2 + p[z]**2)      
    dq = sqrt(q[x]**2 + q[y]**2 + q[z]**2)      
    if dp == 0 or dq == 0:
        return True
    dot = (p[x] * q[x] + p[y] * q[y] + p[z] * q[z]) / dp / dq   
    return abs(1-dot) < 1e-8

class LivePlotter:
    def __init__(self, window):
        self.win = window
        window.live_plot_size = 0
        self.after = None
        self.error_after = None
        self.running = BooleanVar(window)
        self.running.set(False)
        self.lastpts = -1
        self.last_speed = -1
        self.last_limit = None
        self.last_motion_mode = None
        self.last_joint_position = None
        self.set_manual_mode = False
        self.notifications_clear = False
        self.notifications_clear_info = False
        self.notifications_clear_error = False

    def start(self):
        if self.running.get(): return
        if not os.path.exists(emc.nmlfile):
            return False
        try:
            self.stat = emc.stat()
        except emc.error:
            return False
        def C(s):
            a = o.colors[s + "_alpha"]
            s = o.colors[s]
            return [int(x * 255) for x in s + (a,)]

        self.logger = emc.positionlogger(emc.stat(),
            C('backplotjog'),
            C('backplottraverse'),
            C('backplotfeed'),
            C('backplotarc'),
            C('backplottoolchange'),
            C('backplotprobing'),
            geometry
        )
        o.after_idle(lambda: thread.start_new_thread(self.logger.start, (.01,)))

        global feedrate_blackout, spindlerate_blackout, maxvel_blackout
        feedrate_blackout=spindlerate_blackout=maxvel_blackout=time.time()+1

        self.running.set(True)

    def stop(self):
        if not self.running.get(): return
        if hasattr(self, 'stat'): del self.stat
        if self.after is not None:
            self.win.after_cancel(self.after)
            self.after = None
        if self.error_after is not None:
            self.win.after_cancel(self.error_after)
            self.error_after = None
        self.logger.stop()
        self.running.set(True)

    def error_task(self):
        error = e.poll()
        if error: 
            kind, text = error
            if kind in (emc.NML_ERROR, emc.OPERATOR_ERROR):
                icon = "error"
            else:
                icon = "info"
            notifications.add(icon, text)
        self.error_after = self.win.after(200, self.error_task)

    def update(self):
        if not self.running.get():
            return
        try:
            self.stat.poll()
        except emc.error, detail:
            print "error", detail
            del self.stat
            return
        self.after = self.win.after(update_ms, self.update)

        self.win.set_current_line(self.stat.id or self.stat.motion_line)

        speed = self.stat.current_vel

        limits = soft_limits()

        if (self.logger.npts != self.lastpts
                or limits != o.last_limits
                or self.stat.actual_position != o.last_position
                or self.stat.joint_actual_position != o.last_joint_position
                or self.stat.homed != o.last_homed
                or self.stat.origin != o.last_origin
                or self.stat.limit != o.last_limit
                or self.stat.tool_in_spindle != o.last_tool
                or self.stat.motion_mode != o.last_motion_mode
                or abs(speed - self.last_speed) > .01):
            o.redraw_soon()
            o.last_limits = limits
            o.last_limit = self.stat.limit
            o.last_homed = self.stat.homed
            o.last_position = self.stat.actual_position
            o.last_origin = self.stat.origin
            o.last_motion_mode = self.stat.motion_mode
            o.last_tool = self.stat.tool_in_spindle
            o.last_joint_position = self.stat.joint_actual_position
            self.last_speed = speed
            self.lastpts = self.logger.npts

        root_window.update_idletasks()
        vupdate(vars.exec_state, self.stat.exec_state)
        vupdate(vars.interp_state, self.stat.interp_state)
        set_manual_mode = comp["set-manual-mode"]
        if self.set_manual_mode != set_manual_mode:
             self.set_manual_mode = set_manual_mode
             if self.set_manual_mode:
                 root_window.tk.eval(pane_top + ".tabs raise manual")
        notifications_clear = comp["notifications-clear"]
        if self.notifications_clear != notifications_clear:
             self.notifications_clear = notifications_clear
             if self.notifications_clear:
                 notifications.clear()
        notifications_clear_info = comp["notifications-clear-info"]
        if self.notifications_clear_info != notifications_clear_info:
             self.notifications_clear_info = notifications_clear_info
             if self.notifications_clear_info:
                 notifications.clear("info")
        notifications_clear_error = comp["notifications-clear-error"]
        if self.notifications_clear_error != notifications_clear_error:
             self.notifications_clear_error = notifications_clear_error
             if self.notifications_clear_error:
                 notifications.clear("error")
        vupdate(vars.task_mode, self.stat.task_mode)
        vupdate(vars.task_state, self.stat.task_state)
        vupdate(vars.task_paused, self.stat.task_paused)
        vupdate(vars.taskfile, self.stat.file)
        vupdate(vars.interp_pause, self.stat.paused)
        vupdate(vars.mist, self.stat.mist)
        vupdate(vars.flood, self.stat.flood)
        vupdate(vars.brake, self.stat.spindle_brake)
        vupdate(vars.spindledir, self.stat.spindle_direction)
        vupdate(vars.motion_mode, self.stat.motion_mode)
        vupdate(vars.optional_stop, self.stat.optional_stop)
        vupdate(vars.block_delete, self.stat.block_delete)
        if time.time() > spindlerate_blackout:
            vupdate(vars.spindlerate, int(100 * self.stat.spindlerate + .5))
        if time.time() > feedrate_blackout:
            vupdate(vars.feedrate, int(100 * self.stat.feedrate + .5))
        if time.time() > maxvel_blackout:
            m = to_internal_linear_unit(self.stat.max_velocity)
            if vars.metric.get(): m = m * 25.4
            vupdate(vars.maxvel_speed, float(int(600 * m)/10.0))
            root_window.tk.call("update_maxvel_slider")
        vupdate(vars.override_limits, self.stat.joint[0]['override_limits'])
        on_any_limit = 0
        for l in self.stat.limit:
            if l:
                on_any_limit = True
                break
        vupdate(vars.on_any_limit, on_any_limit)
        global current_tool
        current_tool = self.stat.tool_table[0]
        if current_tool:
            tool_data = {'tool': current_tool[0], 'zo': current_tool[1], 'xo': current_tool[2], 'dia': current_tool[3]}
        if current_tool is None:
            vupdate(vars.tool, _("Unknown tool %d") % self.stat.tool_in_spindle)
        elif tool_data['tool'] == 0 or tool_data['tool'] == -1:
            vupdate(vars.tool, _("No tool"))
        elif current_tool.xoffset == 0 and not lathe:
            vupdate(vars.tool, _("Tool %(tool)d, offset %(zo)g, diameter %(dia)g") % tool_data)
        else:
            vupdate(vars.tool, _("Tool %(tool)d, zo %(zo)g, xo %(xo)g, dia %(dia)g") % tool_data)
        active_codes = []
        for i in self.stat.gcodes[1:]:
            if i == -1: continue
            if i % 10 == 0:
                active_codes.append("G%d" % (i/10))
            else:
                active_codes.append("G%(ones)d.%(tenths)d" % {'ones': i/10, 'tenths': i%10})

        for i in self.stat.mcodes[1:]:
            if i == -1: continue
            active_codes.append("M%d" % i)

        feed_str = "F%.1f" % self.stat.settings[1]
        if feed_str.endswith(".0"): feed_str = feed_str[:-2]
        active_codes.append(feed_str)
        active_codes.append("S%.0f" % self.stat.settings[2])

        codes = " ".join(active_codes)
        widgets.code_text.configure(state="normal")
        widgets.code_text.delete("0.0", "end")
        widgets.code_text.insert("end", codes)
        widgets.code_text.configure(state="disabled")

        user_live_update()

    def clear(self):
        self.logger.clear()
        o.redraw_soon()

def running(do_poll=True):
    if do_poll: s.poll()
    return s.task_mode == emc.MODE_AUTO and s.interp_state != emc.INTERP_IDLE

def manual_ok(do_poll=True):
    if do_poll: s.poll()
    if s.task_state != emc.STATE_ON: return False
    return s.interp_state == emc.INTERP_IDLE

# If emc is not already in one of the modes given, switch it to the first
# mode
def ensure_mode(m, *p):
    s.poll()
    if s.task_mode == m or s.task_mode in p: return True
    if running(do_poll=False): return False
    c.mode(m)
    c.wait_complete()
    return True

class DummyProgress:
    def update(self, count): pass
    def nextphase(self, count): pass
    def done(self): pass

class Progress:
    def __init__(self, phases, total):
        self.num_phases = phases
        self.phase = 0
        self.total = total or 1
        self.lastcount = 0
        self.text = None
        self.old_focus = root_window.tk.call("focus", "-lastfor", ".")
        root_window.tk.call("canvas", ".info.progress",
                    "-width", 1, "-height", 1,
                    "-highlightthickness", 0,
                    "-borderwidth", 2, "-relief", "sunken",
                    "-cursor", "watch")
        root_window.configure(cursor="watch")
        root_window.tk.call(".menu", "configure", "-cursor", "watch")
        t.configure(cursor="watch")
        root_window.tk.call("bind", ".info.progress", "<Key>", "break")
        root_window.tk.call("pack", ".info.progress", "-side", "left",
                                "-fill", "both", "-expand", "1")
        root_window.tk.call(".info.progress", "create", "rectangle",
                                (-10, -10, -10, -10),
                                "-fill", "blue", "-outline", "blue")
        root_window.update_idletasks()
        root_window.tk.call("focus", "-force", ".info.progress")
        root_window.tk.call("patient_grab", ".info.progress")

    def update(self, count, force=0):
        if force or count - self.lastcount > 400:
            fraction = (self.phase + count * 1. / self.total) / self.num_phases
            self.lastcount = count
            try:
                width = int(t.tk.call("winfo", "width", ".info.progress"))
            except Tkinter.TclError, detail:
                print detail
                return
            height = int(t.tk.call("winfo", "height", ".info.progress"))
            t.tk.call(".info.progress", "coords", "1",
                (0, 0, int(fraction * width), height))
            t.tk.call("update", "idletasks")

    def nextphase(self, total):
        self.phase += 1
        self.total = total or 1
        self.lastcount = -100

    def done(self):
        root_window.tk.call("destroy", ".info.progress")
        root_window.tk.call("grab", "release", ".info.progress")
        root_window.tk.call("focus", self.old_focus)
        root_window.configure(cursor="")
        root_window.tk.call(".menu", "configure", "-cursor", "")
        t.configure(cursor="xterm")

    def __del__(self):
        if root_window.tk.call("winfo", "exists", ".info.progress"):
            self.done()

    def set_text(self, text):
        if self.text is None:
            self.text = root_window.tk.call(".info.progress", "create", "text",
                (1, 1), "-text", text, "-anchor", "nw")
        else:
            root_window.tk.call(".info.progress", "itemconfigure", text,
                "-text", text)

class AxisCanon(GLCanon, StatMixin):
    def __init__(self, widget, text, linecount, progress):
        GLCanon.__init__(self, widget.colors, geometry)
        StatMixin.__init__(self, s)
        self.text = text
        self.linecount = linecount
        self.progress = progress
        self.aborted = False
        root_window.bind_class(".info.progress", "<Escape>", self.do_cancel)

    def is_lathe(self): return lathe

    def do_cancel(self, event):
        self.aborted = True

    def check_abort(self):
        root_window.update()
        if self.aborted: raise KeyboardInterrupt

    def next_line(self, st):
        GLCanon.next_line(self, st)
        self.progress.update(self.lineno)

progress_re = re.compile("^FILTER_PROGRESS=(\\d*)$")
def filter_program(program_filter, infilename, outfilename):
    import subprocess
    outfile = open(outfilename, "w")
    infilename_q = infilename.replace("'", "'\\''")
    env = dict(os.environ)
    env['AXIS_PROGRESS_BAR'] = '1'
    p = subprocess.Popen(["sh", "-c", "%s '%s'" % (program_filter, infilename_q)],
                          stdin=subprocess.PIPE,
                          stdout=outfile,
                          stderr=subprocess.PIPE,
                          env=env)
    p.stdin.close()  # No input for you
    progress = Progress(1, 100)
    progress.set_text(_("Filtering..."))
    stderr_text = []
    try:
        while p.poll() is None: # XXX add checking for abort
            t.update()
            r,w,x = select.select([p.stderr], [], [], 0.100)
            if r:
                stderr_line = p.stderr.readline()
                m = progress_re.match(stderr_line)
                if m:
                    progress.update(int(m.group(1)), 1)
                else:
                    stderr_text.append(stderr_line)
                    sys.stderr.write(stderr_line)
        # .. might be something left on stderr
        for line in p.stderr:
            m = progress_re.match(line)
            if not m:
                stderr_text.append(line)
                sys.stderr.write(line)
        return p.returncode, "".join(stderr_text)
    finally:
        progress.done()

def get_filter(filename):
    ext = os.path.splitext(filename)[1]
    if ext:
        return inifile.find("FILTER", ext[1:])
    else:
        return None

def update_recent_menu():
    recent = ap.getpref('recentfiles', [], repr)
    root_window.tk.call("update_recent", *recent)

def add_recent_file(f):
    recent = ap.getpref('recentfiles', [], repr)
    if f in recent: recent.remove(f)
    recent.insert(0, f)
    recent = recent[:10]
    ap.putpref('recentfiles', recent, repr)
    update_recent_menu()

loaded_file = None
def open_file_guts(f, filtered=False, addrecent=True):
    if addrecent:
        add_recent_file(f)
    if not filtered:
        global loaded_file
        loaded_file = f
        program_filter = get_filter(f)
        if program_filter:
            tempfile = os.path.join(tempdir, os.path.basename(f))
            exitcode, stderr = filter_program(program_filter, f, tempfile)
            if exitcode:
                root_window.tk.call("nf_dialog", (".error", "-ext", stderr),
                        _("Filter failed"),
                        _("The program %(program)r exited with code %(code)d.  "
                        "Any error messages it produced are shown below:")
                            % {'program': program_filter, 'code': exitcode},
                        "error",0,_("OK"))
                return
            return open_file_guts(tempfile, True, False)

    set_first_line(0)
    t0 = time.time()

    canon = None
    try:
        # be sure to switch modes to cause an interp synch, which
        # writes out the var file.  there was a reset here, and that
        # causes a var file write, but nukes important settings like
        # TLO.
        ensure_mode(emc.MODE_MDI)
        c.wait_complete()
        ensure_mode(emc.MODE_AUTO)
        c.wait_complete()
        c.program_open(f)
        lines = open(f).readlines()
        progress = Progress(4, len(lines)) 
        t.configure(state="normal")
        t.tk.call("delete_all", t)
        code = []
        i = 0
        for i, l in enumerate(lines):
            l = l.expandtabs().replace("\r", "")
            #t.insert("end", "%6d: " % (i+1), "lineno", l)
            code.extend(["%6d: " % (i+1), "lineno", l, ""])
            if i % 1000 == 0:
                t.insert("end", *code)
                del code[:]
                progress.update(i)
        if code:
            t.insert("end", *code)
        progress.nextphase(len(lines))
        f = os.path.abspath(f)
        o.g = canon = AxisCanon(o, widgets.text, i, progress)

        parameter = inifile.find("RS274NGC", "PARAMETER_FILE")
        temp_parameter = os.path.join(tempdir, os.path.basename(parameter))
        shutil.copy(parameter, temp_parameter)
        canon.parameter_file = temp_parameter

        initcode = inifile.find("EMC", "RS274NGC_STARTUP_CODE") or ""
	if initcode == "":
    	    initcode = inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""
        unitcode = "G%d" % (20 + (s.linear_units == 1))
        try:
            result, seq = gcode.parse(f, canon, unitcode, initcode)
        except KeyboardInterrupt:
            result, seq = 0, 0
        # According to the documentation, MIN_ERROR is the largest value that is
        # not an error.  Crazy though that sounds...
        if result > gcode.MIN_ERROR:
            error_str = _(gcode.strerror(result))
            root_window.tk.call("nf_dialog", ".error",
                    _("G-Code error in %s") % os.path.basename(f),
                    _("Near line %(seq)d of %(f)s:\n%(error_str)s") % {'seq': seq, 'f': f, 'error_str': error_str},
                    "error",0,_("OK"))

        t.configure(state="disabled")

        canon.calc_extents()
        canon.calc_notool_extents()
        make_main_list(canon)
        make_selection_list(canon)

    finally:
        # Before unbusying, I update again, so that any keystroke events
        # that reached the program while it was busy are sent to the
        # label, not to another window in the application.  If this
        # update call is removed, the events are only handled after that
        # widget is destroyed and focus has passed to some other widget,
        # which will handle the keystrokes instead, leading to the
        # R-while-loading bug.
        #print "load_time", time.time() - t0
        root_window.update()
        root_window.tk.call("destroy", ".info.progress")
        root_window.tk.call("grab", "release", ".info.progress")
        if canon:
            canon.progress = DummyProgress()
        try:
            progress.done()
        except UnboundLocalError:
            pass
        o.tkRedraw()
        root_window.tk.call("set_mode_from_tab")

tabs_mdi = str(root_window.tk.call("set", "_tabs_mdi"))
tabs_manual = str(root_window.tk.call("set", "_tabs_manual"))
tabs_preview = str(root_window.tk.call("set", "_tabs_preview"))
tabs_numbers = str(root_window.tk.call("set", "_tabs_numbers"))
pane_top = str(root_window.tk.call("set", "pane_top"))
pane_bottom = str(root_window.tk.call("set", "pane_bottom"))
widgets = nf.Widgets(root_window, 
    ("help_window", Toplevel, ".keys"),
    ("about_window", Toplevel, ".about"),
    ("text", Text, pane_bottom + ".t.text"),
    ("preview_frame", Frame, tabs_preview),
    ("numbers_text", Text, tabs_numbers + ".text"),
    ("tabs", bwidget.NoteBook, pane_top + ".tabs"),
    ("right", bwidget.NoteBook, pane_top + ".right"),
    ("mdi_history", Listbox, tabs_mdi + ".history"),
    ("mdi_command", Entry, tabs_mdi + ".command"),
    ("code_text", Text, tabs_mdi + ".gcodes"),

    ("axes", Radiobutton, tabs_manual + ".axes"),
    ("axis_x", Radiobutton, tabs_manual + ".axes.axisx"),
    ("axis_y", Radiobutton, tabs_manual + ".axes.axisy"),
    ("axis_z", Radiobutton, tabs_manual + ".axes.axisz"),
    ("axis_a", Radiobutton, tabs_manual + ".axes.axisa"),
    ("axis_b", Radiobutton, tabs_manual + ".axes.axisb"),
    ("axis_c", Radiobutton, tabs_manual + ".axes.axisc"),
    ("axis_u", Radiobutton, tabs_manual + ".axes.axisu"),
    ("axis_v", Radiobutton, tabs_manual + ".axes.axisv"),
    ("axis_w", Radiobutton, tabs_manual + ".axes.axisw"),

    ("joints", Radiobutton, tabs_manual + ".joints"),
    ("joint_0", Radiobutton, tabs_manual + ".joints.joint0"),
    ("joint_1", Radiobutton, tabs_manual + ".joints.joint1"),
    ("joint_2", Radiobutton, tabs_manual + ".joints.joint2"),
    ("joint_3", Radiobutton, tabs_manual + ".joints.joint3"),
    ("joint_4", Radiobutton, tabs_manual + ".joints.joint4"),
    ("joint_5", Radiobutton, tabs_manual + ".joints.joint5"),
    ("joint_6", Radiobutton, tabs_manual + ".joints.joint6"),
    ("joint_7", Radiobutton, tabs_manual + ".joints.joint7"),
    ("joint_8", Radiobutton, tabs_manual + ".joints.joint8"),
    ("joint_9", Radiobutton, tabs_manual + ".joints.joint9"),

    ("jogincr", Entry, tabs_manual + ".jogf.jog.jogincr"),
    ("override", Checkbutton, tabs_manual + ".jogf.override"),

    ("ajogspeed", Entry, pane_top + ".ajogspeed"),

    ("lubel", Label, tabs_manual + ".coolant"),
    ("flood", Checkbutton, tabs_manual + ".flood"),
    ("mist", Checkbutton, tabs_manual + ".mist"),

    ("brake", Checkbutton, tabs_manual + ".spindlef.brake"),

    ("spindlel", Label, tabs_manual + ".spindlel"),
    ("spindlef", Frame, tabs_manual + ".spindlef"),
    ("spindle_ccw", Radiobutton, tabs_manual + ".spindlef.ccw"),
    ("spindle_stop", Radiobutton, tabs_manual + ".spindlef.stop"),
    ("spindle_cw", Radiobutton, tabs_manual + ".spindlef.cw"),

    ("spindle_minus", Button, tabs_manual + ".spindlef.spindleminus"),
    ("spindle_plus", Button, tabs_manual + ".spindlef.spindleplus"),

    ("view_z", Button, ".toolbar.view_z"),
    ("view_z2", Button, ".toolbar.view_z2"),
    ("view_x", Button, ".toolbar.view_x"),
    ("view_y", Button, ".toolbar.view_y"),
    ("view_p", Button, ".toolbar.view_p"),
    ("rotate", Button, ".toolbar.rotate"),

    ("feedoverride", Scale, pane_top + ".feedoverride.foscale"),
    ("spinoverride", Scale, pane_top + ".spinoverride.foscale"),
    ("spinoverridef", Scale, pane_top + ".spinoverride"),

    ("menu_view", Menu, ".menu.view"),
    ("menu_file", Menu, ".menu.file"),
    ("menu_machine", Menu, ".menu.machine"),
    ("menu_touchoff", Menu, ".menu.machine.touchoff"),

    ("homebutton", Button, tabs_manual + ".jogf.zerohome.home"),
    ("homemenu", Menu, ".menu.machine.home"),
    ("unhomemenu", Menu, ".menu.machine.unhome")
)

def activate_axis(i, force=0):
    if not force and not manual_ok(): return
    if joints_mode():
        if i >= num_joints: return
        axis = getattr(widgets, "joint_%d" % i)
    else:
        if not s.axis_mask & (1<<i): return
        axis = getattr(widgets, "axis_%s" % "xyzabcuvw"[i])
    axis.focus()
    axis.invoke()

def set_first_line(lineno):
    global program_start_line
    program_start_line = lineno
    t.tag_remove("ignored", "0.0", "end")
    if lineno > 0:
        t.tag_add("ignored", "0.0", "%d.end" % (lineno-1))

def parse_increment(jogincr):
    if jogincr.endswith("mm"):
        scale = from_internal_linear_unit(1/25.4)
    elif jogincr.endswith("cm"):
        scale = from_internal_linear_unit(10/25.4)
    elif jogincr.endswith("um"):
        scale = from_internal_linear_unit(.001/25.4)
    elif jogincr.endswith("in") or jogincr.endswith("inch"):
        scale = from_internal_linear_unit(1.)
    elif jogincr.endswith("mil"):
        scale = from_internal_linear_unit(.001)
    else:
        scale = 1
    jogincr = jogincr.rstrip(" inchmuil")
    if "/" in jogincr:
        p, q = jogincr.split("/")
        jogincr = float(p) / float(q)
    else:
        jogincr = float(jogincr)
    return jogincr * scale


def set_hal_jogincrement():
    if not 'comp' in globals(): return # this is called once during startup before comp exists
    jogincr = widgets.jogincr.get()
    if jogincr == _("Continuous"):
        distance = 0
    else:
        distance = parse_increment(jogincr)
    comp['jog.increment'] = distance

def jogspeed_listbox_change(dummy, value):
    global jogincr_index_last
    # pdb.set_trace()
    # FJ: curselection is not always up to date here, so 
    #     do a linear search by hand
    iterator = iter(root_window.call(widgets.jogincr._w, "list", "get", "0", "end"))
    idx = 0
    cursel = -1
    for i in iterator:
        if i == unicode(value, 'utf-8'):
            cursel= idx
            break
        idx += 1
    if cursel > 0:
        jogincr_index_last= cursel
    set_hal_jogincrement()

def jogspeed_continuous():
    root_window.call(widgets.jogincr._w, "select", 0)

def jogspeed_incremental(dir=1):
    global jogincr_index_last
    jogincr_size = int(root_window.call(widgets.jogincr._w, "list", "size"))
    # pdb.set_trace()
    cursel = root_window.call(widgets.jogincr._w, "curselection")
    if cursel == "":
        cursel = 0
    else:
        cursel = int(cursel)
    if dir == 1:
        if cursel > 0:
            # If it was "Continous" just before, then don't change last jog increment!
            jogincr_index_last += 1
        if jogincr_index_last >= jogincr_size:
            jogincr_index_last = jogincr_size - 1
    else:
        if cursel > 0:
            jogincr_index_last -= 1
        if jogincr_index_last < 1:
            jogincr_index_last = 1
    root_window.call(widgets.jogincr._w, "select", jogincr_index_last)   
    set_hal_jogincrement()


class SelectionHandler:
    def __init__(self, win, **kw):
        self.kw = kw
        self.win = win
        self.win.selection_handle(self.handler, *kw)
        self.value = ""

    def set_value(self, value):
        self.win.selection_own(**self.kw)
        self.value = value

    def handler(self, offset, maxchars):
        offset = int(offset)
        maxchars = int(maxchars)
        return self.value[offset:offset+maxchars]

selection = SelectionHandler(root_window)

class DummyCanon:
    def comment(*args): pass
    def next_line(*args): pass
    def set_origin_offsets(*args): pass
    def set_xy_rotation(*args): pass
    def get_external_angular_units(self): return 1.0
    def get_external_length_units(self): return 1.0
    def set_plane(*args): pass
    def get_axis_mask(self): return 7
    def get_tool(self, tool):
        return tool,0.,0.,0.,0.,0.,0
    def set_feed_rate(self, rate): pass

    def user_defined_function(self, m, p, q):
        self.number = p

class _prompt_float:
    """ Prompt for a g-code floating point expression """
    def __init__(self, title, text, default, unit_str=''):
        self.unit_str = unit_str
        t = self.t = Toplevel(root_window, padx=7, pady=7)
        t.wm_title(title)
        t.wm_transient(root_window)
        t.wm_resizable(0, 0)
        m = Message(t, text=text, aspect=500, anchor="w", justify="left")
        self.v = v = StringVar(t)
        self.u = u = BooleanVar(t)
        self.w = w = StringVar(t)
        l = Tkinter.Message(t, textvariable=w, justify="left", anchor="w",
                aspect=500)
        v.set(default)
        self.e = e = Entry(t, textvariable=v)
        self.buttons = f = Tkinter.Frame(t)
        self.ok = Tkinter.Button(f, text=_("OK"), command=self.do_ok, width=10,height=1,padx=0,pady=.25, default="active")
        self.cancel = Tkinter.Button(f, text=_("Cancel"), command=self.do_cancel, width=10,height=1,padx=0,pady=.25, default="normal")
        v.trace("w", self.check_valid)
        t.wm_protocol("WM_DELETE_WINDOW", self.cancel.invoke)
        t.bind("<Return>", lambda event: (self.ok.flash(), self.ok.invoke()))
        t.bind("<KP_Enter>", lambda event: (self.ok.flash(), self.ok.invoke()))
        t.bind("<Escape>", lambda event: (self.cancel.flash(), self.cancel.invoke()))

        m.pack(side="top", anchor="w")
        e.pack(side="top", anchor="e")
        l.pack(side="top", anchor="w", fill="x", expand=1)
        f.pack(side="bottom", anchor="e")
        self.ok.pack(side="left", padx=3, pady=3)
        self.cancel.pack(side="left", padx=3, pady=3)

    def do_ok(self):
        self.u.set(True)
        self.t.destroy()

    def do_cancel(self):
        self.u.set(False)
        self.t.destroy()

    def check_valid(self, *args):
        v = self.v.get()

        st = 0
        ok = 1

        if "#" in v:
            ok = 0
            self.w.set("Variables may not be used here")

        if ok:
            for ch in v:
                if ch == "[": st += 1
                elif ch == "]": st -= 1
                if st < 0:
                    ok = 0
                    self.w.set("Right bracket without matching left bracket")
                    break
            if st != 0:
                self.w.set("Left bracket without matching right bracket")
                ok = 0

        if ok:
            f = os.path.devnull
            canon = DummyCanon()

            parameter = inifile.find("RS274NGC", "PARAMETER_FILE")
            temp_parameter = os.path.join(tempdir, os.path.basename(parameter))
            shutil.copy(parameter, temp_parameter)
            canon.parameter_file = temp_parameter

            result, seq = gcode.parse("", canon, "M199 P["+v+"]", "M2")

            if result > gcode.MIN_ERROR:
                self.w.set(gcode.strerror(result))
                ok = 0
            else:
                self.w.set("= %f%s" % (canon.number, self.unit_str))

        if ok: 
            self.ok.configure(state="normal")
        else:
            self.ok.configure(state="disabled")

    def do_focus(self):
        if not self.e.winfo_viewable():
            self._after = self.t.after(10, self.do_focus)
        else:
            self.e.focus()
            self.e.selection_range(0, "end")
            self._after = None

    def result(self):
        if self.u.get(): return self.v.get()
        return None

    def run(self):
        self.t.grab_set()
        self._after = self.t.after_idle(self.do_focus)
        self.t.wait_window()
        if self._after is not None:
            self.t.after_cancel(self._after)
        try:
            self.t.destroy()
        except Tkinter.TclError:
            pass
        return self.result()

def prompt_float(title, text, default, unit_str):
    t = _prompt_float(title, text, default, unit_str)
    return t.run()

all_systems = ['P1  G54', 'P2  G55', 'P3  G56', 'P4  G57', 'P5  G58',
            'P6  G59', 'P7  G59.1', 'P8  G59.2', 'P9  G59.3',
            _('T    Tool Table')]

class _prompt_touchoff(_prompt_float):
    def __init__(self, title, text, default, defaultsystem):
        systems = all_systems[:]
        if s.tlo_is_along_w:
            tool_offset_axes = "w"
        elif lathe:
            tool_offset_axes = "xz"
        else:
            tool_offset_axes = "z"
        if vars.current_axis.get() not in tool_offset_axes:
            del systems[-1]
            if defaultsystem.startswith("T"): defaultsystem = systems[0]
        if s.tool_in_spindle == 0:
            del systems[-1]
        linear_axis = vars.current_axis.get() in "xyzuvw"
        if linear_axis:
            if vars.metric.get(): unit_str = " " + _("mm")
            else: unit_str = " " + _("in")
            if lathe and vars.current_axis.get() == "x":
                if 80 in s.gcodes:
                    unit_str += _(" radius")
                else:
                    unit_str += _(" diameter")
        else: unit_str = _(u"\xb0")
        _prompt_float.__init__(self, title, text, default, unit_str)
        t = self.t
        f = Frame(t)
        self.c = c = StringVar(t)
        c.set(defaultsystem)
        l = Label(f, text=_("Coordinate System:"))
        mb = OptionMenu(f, c, *systems)
        mb.tk.call("size_menubutton_to_entries", mb)
        mb.configure(takefocus=1)
        l.pack(side="left") 
        mb.pack(side="left")
        f.pack(side="top") 
        self.buttons.tkraise()
        for i in [1,2,3,4,5,6,7,8,9]:
            t.bind("<Alt-KeyPress-%s>" % i, lambda event, system=systems[i-1]: c.set(system))
        if not ((s.tool_in_spindle == 0) or vars.current_axis.get() not in tool_offset_axes):
            t.bind("<Alt-t>", lambda event: c.set(systems[9]))
            t.bind("<Alt-0>", lambda event: c.set(systems[9]))

    def result(self):
        if self.u.get(): return self.v.get(), self.c.get()
        return None, None
        
def prompt_touchoff(title, text, default, system=None):
    t = _prompt_touchoff(title, text, default, system)
    return t.run()

property_names = [
    ('name', _("Name:")), ('size', _("Size:")),
    ('tools', _("Tool order:")), ('g0', _("Rapid distance:")),
    ('g1', _("Feed distance:")), ('g', _("Total distance:")),
    ('run', _("Run time:")), ('x', _("X bounds:")),
    ('y', _("Y bounds:")), ('z', _("Z bounds:")),
    ('a', _("A bounds:")), ('b', _("B bounds:")),
    ('c', _("C bounds:"))
]

def dist((x,y,z),(p,q,r)):
    return ((x-p)**2 + (y-q)**2 + (z-r)**2) ** .5

# returns units/sec
def get_jog_speed(a):
    if vars.joint_mode.get() or a in (0,1,2,6,7,8):
        return vars.jog_speed.get()/60.
    else: return vars.jog_aspeed.get()/60.

def get_max_jog_speed(a):
    if vars.joint_mode.get() or a in (0,1,2,6,7,8):
        return vars.max_speed.get()
    else: return vars.max_aspeed.get()    

def run_warn():
    warnings = []
    if o.g:
        machine_limit_min, machine_limit_max = soft_limits()
        for i in range(3): # Does not enforce angle limits
            if not(s.axis_mask & (1<<i)): continue
            if o.g.min_extents_notool[i] < machine_limit_min[i]:
                warnings.append(_("Program exceeds machine minimum on axis %s")
                    % "XYZABCUVW"[i])
            if o.g.max_extents_notool[i] > machine_limit_max[i]:
                warnings.append(_("Program exceeds machine maximum on axis %s")
                    % "XYZABCUVW"[i])
    if warnings:
        text = "\n".join(warnings)
        return int(root_window.tk.call("nf_dialog", ".error",
            _("Program exceeds machine limits"),
            text,
            "warning",
            1, _("Run Anyway"), _("Cancel")))
    return 0

def reload_file(refilter=True):
    if running(): return
    s.poll()
    if not loaded_file:
        root_window.tk.call("set_mode_from_tab")
        return
    line = vars.highlight_line.get()
    o.set_highlight_line(None)

    if refilter or not get_filter(loaded_file):
        open_file_guts(loaded_file, False, False)
    else:
        tempfile = os.path.join(tempdir, os.path.basename(loaded_file))
        open_file_guts(tempfile, True, False)
    if line:
        o.set_highlight_line(line)
 
class TclCommands(nf.TclCommands):
    def toggle_preview(event=None):
        current = widgets.right.raise_page()
        if current == "preview": widgets.right.raise_page("numbers")
        else: widgets.right.raise_page("preview")
    def redraw_soon(event=None):
        o.redraw_soon()

    def to_internal_linear_unit(a, b=None):
        if b is not None: b = float(b)
        return to_internal_linear_unit(float(a), b)
    def from_internal_linear_unit(a, b=None):
        if b is not None: b = float(b)
        return from_internal_linear_unit(float(a), b)

    def toggle_optional_stop(event=None):
        c.set_optional_stop(vars.optional_stop.get())
        ap.putpref("optional_stop", vars.optional_stop.get())

    def toggle_block_delete(event=None):
        c.set_block_delete(vars.block_delete.get())
        ap.putpref("block_delete", vars.block_delete.get())
        c.wait_complete()
        ensure_mode(emc.MODE_MANUAL)
        s.poll()
        o.tkRedraw()
        reload_file(False)


    def gcode_properties(event=None):
        props = {}
        if not loaded_file:
            props['name'] = _("No file loaded")
        else:
            ext = os.path.splitext(loaded_file)[1]
            program_filter = None
            if ext:
                program_filter = inifile.find("FILTER", ext[1:])
            name = os.path.basename(loaded_file)
            if program_filter:
                props['name'] = _("generated from %s") % name
            else:
                props['name'] = name

            size = os.stat(loaded_file).st_size
            lines = int(widgets.text.index("end").split(".")[0])-2
            props['size'] = _("%(size)s bytes\n%(lines)s gcode lines") % {'size': size, 'lines': lines}

            if vars.metric.get():
                conv = 1
                units = _("mm")
                fmt = "%.3f"
            else:
                conv = 1/25.4
                units = _("in")
                fmt = "%.4f"

            mf = vars.max_speed.get()
            #print o.g.traverse[0]

            g0 = sum(dist(l[1][:3], l[2][:3]) for l in o.g.traverse)
            g1 = (sum(dist(l[1][:3], l[2][:3]) for l in o.g.feed) +
                sum(dist(l[1][:3], l[2][:3]) for l in o.g.arcfeed))
            gt = (sum(dist(l[1][:3], l[2][:3])/min(mf, l[3]) for l in o.g.feed) +
                sum(dist(l[1][:3], l[2][:3])/min(mf, l[3])  for l in o.g.arcfeed) +
                sum(dist(l[1][:3], l[2][:3])/mf  for l in o.g.traverse) +
                o.g.dwell_time
                )
 
            props['g0'] = "%f %s".replace("%f", fmt) % (from_internal_linear_unit(g0, conv), units)
            props['g1'] = "%f %s".replace("%f", fmt) % (from_internal_linear_unit(g1, conv), units)
            if gt > 120:
                props['run'] = _("%.1f minutes") % (gt/60)
            else:
                props['run'] = _("%d seconds") % (int(gt))

            min_extents = from_internal_units(o.g.min_extents, conv)
            max_extents = from_internal_units(o.g.max_extents, conv)
            for (i, c) in enumerate("xyz"):
                a = min_extents[i]
                b = max_extents[i]
                if a != b:
                    props[c] = _("%(a)f to %(b)f = %(diff)f %(units)s").replace("%f", fmt) % {'a': a, 'b': b, 'diff': b-a, 'units': units}
        properties(root_window, _("G-Code Properties"), property_names, props)

    def launch_website(event=None):
        import webbrowser
        webbrowser.open("http://www.linuxcnc.org/")

    def set_spindlerate(newval):
        global spindlerate_blackout
        try:
            value = int(newval)
        except ValueError: return
        value = value / 100.
        c.spindleoverride(value)
        spindlerate_blackout = time.time() + 1

    def set_feedrate(newval):
        global feedrate_blackout
        try:
            value = int(newval)
        except ValueError: return
        value = value / 100.
        c.feedrate(value)
        feedrate_blackout = time.time() + 1

    def set_maxvel(newval):
        newval = float(newval)
        if vars.metric.get(): newval = newval / 25.4
        newval = from_internal_linear_unit(newval)
        global maxvel_blackout
        c.maxvel(newval / 60.)
        maxvel_blackout = time.time() + 1

    def copy_line(*args):
        line = -1
        if vars.running_line.get() != -1: line = vars.running_line.get()
        if vars.highlight_line.get() != -1: line = vars.highlight_line.get()
        if line == -1: return
        selection.set_value(t.get("%d.8" % line, "%d.end" % line))

    def task_run_line(*args):
        line = vars.highlight_line.get()
        if line != -1: set_first_line(line)
        commands.task_run()

    def reload_tool_table(*args):
        c.load_tool_table()

    def program_verify(*args):
        set_first_line(-1)
        commands.task_run()

    def zoomin(event=None):
        o.zoomin(event)

    def zoomout(event=None):
        o.zoomout(event)

    def set_view_x(event=None):
        widgets.view_z.configure(relief="link")
        widgets.view_z2.configure(relief="link")
        widgets.view_x.configure(relief="sunken")
        widgets.view_y.configure(relief="link")
        widgets.view_p.configure(relief="link")
        vars.view_type.set(3)
        o.reset()
        glRotatef(-90, 0, 1, 0)
        glRotatef(-90, 1, 0, 0)
        if o.g:
            mid = [(a+b)/2 for a, b in zip(o.g.max_extents, o.g.min_extents)]
            glTranslatef(-mid[0], -mid[1], -mid[2])
            size = [(a-b) for a, b in zip(o.g.max_extents, o.g.min_extents)]
            o.set_eyepoint_from_extents(size[1], size[2])
        else:
            o.set_eyepoint(5.)
        o.perspective = False
        o.lat = -90
        o.lon = 270
        o.tkRedraw()

    def set_view_y(event=None):
        widgets.view_z.configure(relief="link")
        widgets.view_z2.configure(relief="link")
        widgets.view_x.configure(relief="link")
        widgets.view_y.configure(relief="sunken")
        widgets.view_p.configure(relief="link")
        vars.view_type.set(4)
        o.reset()
        glRotatef(-90, 1, 0, 0)
        if lathe:
            glRotatef(90, 0, 1, 0)
        if o.g:
            mid = [(a+b)/2 for a, b in zip(o.g.max_extents, o.g.min_extents)]
            glTranslatef(-mid[0], -mid[1], -mid[2])
            size = [(a-b) for a, b in zip(o.g.max_extents, o.g.min_extents)]
            o.set_eyepoint_from_extents(size[0], size[2])
        else:
            o.set_eyepoint(5.)
        o.perspective = False
        o.lat = -90
        o.lon = 0
        o.tkRedraw()

    def set_view_z(event=None):
        widgets.view_z.configure(relief="sunken")
        widgets.view_z2.configure(relief="link")
        widgets.view_x.configure(relief="link")
        widgets.view_y.configure(relief="link")
        widgets.view_p.configure(relief="link")
        vars.view_type.set(1)
        o.reset()
        if o.g:
            mid = [(a+b)/2 for a, b in zip(o.g.max_extents, o.g.min_extents)]
            glTranslatef(-mid[0], -mid[1], -mid[2])
            size = [(a-b) for a, b in zip(o.g.max_extents, o.g.min_extents)]
            o.set_eyepoint_from_extents(size[0], size[1])
        else:
            o.set_eyepoint(5.)
        o.perspective = False
        o.lat = o.lon = 0
        o.tkRedraw()

    def set_view_z2(event=None):
        widgets.view_z.configure(relief="link")
        widgets.view_z2.configure(relief="sunken")
        widgets.view_x.configure(relief="link")
        widgets.view_y.configure(relief="link")
        widgets.view_p.configure(relief="link")
        vars.view_type.set(2)
        o.reset()
        glRotatef(-90, 0, 0, 1)
        if o.g:
            mid = [(a+b)/2 for a, b in zip(o.g.max_extents, o.g.min_extents)]
            glTranslatef(-mid[0], -mid[1], -mid[2])
            size = [(a-b) for a, b in zip(o.g.max_extents, o.g.min_extents)]
            o.set_eyepoint_from_extents(size[1], size[0])
        else:
            o.set_eyepoint(5.)
        o.perspective = False
        o.lat = 0
        o.lon = 270
        o.tkRedraw()


    def set_view_p(event=None):
        widgets.view_z.configure(relief="link")
        widgets.view_z2.configure(relief="link")
        widgets.view_x.configure(relief="link")
        widgets.view_y.configure(relief="link")
        widgets.view_p.configure(relief="sunken")
        vars.view_type.set(5)
        o.reset()
        o.perspective = True
        if o.g:
            mid = [(a+b)/2 for a, b in zip(o.g.max_extents, o.g.min_extents)]
            glTranslatef(-mid[0], -mid[1], -mid[2])

            size = [(a-b) for a, b in zip(o.g.max_extents, o.g.min_extents)]
            size = sqrt(size[0] **2 + size[1] ** 2 + size[2] ** 2)
            if size > 1e99: size = 5. # in case there are no moves in the preview
            w = o.winfo_width()
            h = o.winfo_height()
            fovx = o.fovy * w / h
            fov = min(fovx, o.fovy)
            o.set_eyepoint((size * 1.1 + 1.0) / 2 / sin ( fov * pi / 180 / 2))
            o.lat = -60
            o.lon = 335
            x = (o.g.min_extents[0] + o.g.max_extents[0])/2
            y = (o.g.min_extents[1] + o.g.max_extents[1])/2
            z = (o.g.min_extents[2] + o.g.max_extents[2])/2
            glRotateScene(o, 1.0, x, y, z, 0, 0, 0, 0)
        else:
            o.set_eyepoint(5.)
            o.lat = -60
            o.lon = 335
            glRotateScene(o, 1.0, o.xcenter, o.ycenter, o.zcenter, 0, 0, 0, 0)
        o.tkRedraw()

    def estop_clicked(event=None):
        s.poll()
        if s.task_state == emc.STATE_ESTOP:
            c.state(emc.STATE_ESTOP_RESET)
        else:
            c.state(emc.STATE_ESTOP)

    def onoff_clicked(event=None):
        s.poll()
        if s.task_state == emc.STATE_ESTOP_RESET:
            c.state(emc.STATE_ON)
        else:
            c.state(emc.STATE_OFF)

    def open_file(*event):
        if running(): return
        global open_directory
        all_extensions = tuple([".ngc"])
        for e in extensions:
            all_extensions = all_extensions + tuple(e[1])
        types = (
            (_("All machinable files"), all_extensions),
            (_("rs274ngc files"), ".ngc")) + extensions + \
            ((_("All files"), "*"),)
        f = root_window.tk.call("tk_getOpenFile", "-initialdir", open_directory,
            "-filetypes", types)
        if not f: return
        o.set_highlight_line(None)
        f = str(f)
        open_directory = os.path.dirname(f)
        commands.open_file_name(f)

    def open_file_name(f):
        open_file_guts(f)
        if str(widgets.view_x['relief']) == "sunken":
            commands.set_view_x()
        elif str(widgets.view_y['relief']) == "sunken":
            commands.set_view_y()
        elif str(widgets.view_z['relief']) == "sunken":
            commands.set_view_z()
        elif  str(widgets.view_z2['relief']) == "sunken":
            commands.set_view_z2()
        else:
            commands.set_view_p()
        if o.g is not None:
            x = (o.g.min_extents[0] + o.g.max_extents[0])/2
            y = (o.g.min_extents[1] + o.g.max_extents[1])/2
            z = (o.g.min_extents[2] + o.g.max_extents[2])/2
            o.set_centerpoint(x, y, z)

    def open_pipe(f, c):
        try:
            os.makedirs(os.path.join(tempdir, "pipe"))
        except os.error:
            pass
        f = os.path.join(tempdir, "pipe", os.path.basename(f))
        fi = open(f, "w")
        fi.write(c)
        fi.close()
        commands.open_file_name(f)

    def reload_file(*event):
        reload_file()

    def edit_program(*event):
        if loaded_file is None:
            pass
        else:
            e = string.split(editor)
            e.append(loaded_file)
            os.spawnvp(os.P_NOWAIT, e[0], e)

    def edit_tooltable(*event):
        if tooltable is None:
            pass
        else:
            e = string.split(tooleditor)
            e.append(tooltable)
            os.spawnvp(os.P_NOWAIT, e[0], e)

    def task_run(*event):
        if run_warn(): return

        global program_start_line, program_start_line_last
        program_start_line_last = program_start_line;
        ensure_mode(emc.MODE_AUTO)
        c.auto(emc.AUTO_RUN, program_start_line)
        program_start_line = 0
        t.tag_remove("ignored", "0.0", "end")
        o.set_highlight_line(None)

    def task_step(*event):
        if s.task_mode != emc.MODE_AUTO or s.interp_state != emc.INTERP_IDLE:
            o.set_highlight_line(None)
            if run_warn(): return
        ensure_mode(emc.MODE_AUTO)
        c.auto(emc.AUTO_STEP)

    def task_pause(*event):
        if s.task_mode != emc.MODE_AUTO or s.interp_state not in (emc.INTERP_READING, emc.INTERP_WAITING):
            return
        ensure_mode(emc.MODE_AUTO)
        c.auto(emc.AUTO_PAUSE)

    def task_resume(*event):
        s.poll()
        if not s.paused:
            return
        if s.task_mode not in (emc.MODE_AUTO, emc.MODE_MDI):
            return
        ensure_mode(emc.MODE_AUTO, emc.MODE_MDI)
        c.auto(emc.AUTO_RESUME)

    def task_pauseresume(*event):
        if s.task_mode not in (emc.MODE_AUTO, emc.MODE_MDI):
            return
        ensure_mode(emc.MODE_AUTO, emc.MODE_MDI)
        s.poll()
        if s.paused:
            c.auto(emc.AUTO_RESUME)
        elif s.interp_state != emc.INTERP_IDLE:
            c.auto(emc.AUTO_PAUSE)

    def task_stop(*event):
        if s.task_mode == emc.MODE_AUTO and vars.running_line.get() != 0:
            o.set_highlight_line(vars.running_line.get())
        c.abort()
        c.wait_complete()

    def mdi_up_cmd(*args):
        if args and args[0].char: return   # e.g., for KP_Up with numlock on
        global mdi_history_index
        if widgets.mdi_command.cget("state") == "disabled":
            return
        if mdi_history_index != -1:
            if mdi_history_index > 0:
                mdi_history_index -= 1
            else:
                mdi_history_index = widgets.mdi_history.size() - 1
            widgets.mdi_history.selection_clear(0, "end")
            widgets.mdi_history.see(mdi_history_index)
            if mdi_history_index != (widgets.mdi_history.size() - 1):
                widgets.mdi_history.selection_set(mdi_history_index, mdi_history_index)
            vars.mdi_command.set(widgets.mdi_history.get(mdi_history_index))
            widgets.mdi_command.selection_range(0, "end")

    def mdi_down_cmd(*args):
        if args and args[0].char: return   # e.g., for KP_Up with numlock on
        global mdi_history_index
        if widgets.mdi_command.cget("state") == "disabled":
            return
        history_size = widgets.mdi_history.size()
        if mdi_history_index != -1:
            if mdi_history_index < (history_size - 1):
                mdi_history_index += 1
            else:
                mdi_history_index = 0
            widgets.mdi_history.selection_clear(0, "end")
            widgets.mdi_history.see(mdi_history_index)
            if mdi_history_index != (widgets.mdi_history.size() - 1):
                widgets.mdi_history.selection_set(mdi_history_index, mdi_history_index)
            vars.mdi_command.set(widgets.mdi_history.get(mdi_history_index))
            widgets.mdi_command.selection_range(0, "end")

    def send_mdi(*event):
        if not manual_ok(): return "break"
        command = vars.mdi_command.get()
        commands.send_mdi_command(command)
        return "break"

    def send_mdi_command(command):
        global mdi_history_index, mdi_history_save_filename
        if command != "":
            command= command.lstrip().rstrip()
            vars.mdi_command.set("")
            ensure_mode(emc.MODE_MDI)
            widgets.mdi_history.selection_clear(0, "end")
            ## check if input is already in list. If so, then delete old element
            #idx = 0
            #for ele in widgets.mdi_history.get(0, "end"):
            #    if ele == command:
            #        widgets.mdi_history.delete(idx)
            #        break
            #    idx += 1
            history_size = widgets.mdi_history.size()
            new_entry = 1
            if history_size > 1 and widgets.mdi_history.get(history_size - 2) == command:
                new_entry = 0
            if new_entry != 0:
                # if command is already at end of list, don't add it again
                widgets.mdi_history.insert(history_size - 1, "%s" % command)
                history_size += 1
            widgets.mdi_history.see(history_size - 1)
            if history_size > (mdi_history_max_entries + 1):
                widgets.mdi_history.delete(0, 0)
                history_size= (mdi_history_max_entries + 1)
            # pdb.set_trace()
            mdi_history_index = widgets.mdi_history.index("end") - 1
            c.mdi(command)
            o.tkRedraw()
            commands.mdi_history_write_to_file(mdi_history_save_filename, history_size)

    # write out mdi history file (history_size equal to -1 will delete history)
    def mdi_history_write_to_file(file_name, history_size):
        # print "mdi_history_write: %s : %d" % (file_name, history_size)
        if history_size > 1 or history_size == -1:
            file_name = os.path.expanduser(file_name)
            try:
                f = open(file_name, "w")
                try:
                    if history_size != -1:
                        for idx in range(history_size - 1):
                            f.write("%s\n" % widgets.mdi_history.get(idx, idx))
                finally:    
                    f.close()
            except IOError:
                print >>sys.stderr, "Can't open MDI history file [%s] for writing" % file_name

    def mdi_history_hist2clip(*event):
        cursel = widgets.mdi_history.curselection()
        root_window.clipboard_clear()
        selection = ""
        count = 0
        if cursel != "":
            for data in cursel:
                selection += "%s\n" % widgets.mdi_history.get(data)
                count += 1
            if selection != "":
                root_window.clipboard_append(selection, type = "STRING")

    def mdi_history_clip2hist(*event):
        try:
            history_size = widgets.mdi_history.size()
            vars.mdi_command.set("")
            count = 0
            for data in root_window.selection_get(selection="CLIPBOARD").split("\n"):
                if data != "":
                    history_size = widgets.mdi_history.size()
                    new_entry = 1
                    if history_size > 1 and widgets.mdi_history.get(history_size - 2) == data:
                        new_entry = 0
                    if new_entry != 0:
                        # if command is already at end of list, don't add it again
                        widgets.mdi_history.insert(history_size - 1, "%s" % data)
                        history_size += 1
                        count += 1
                        widgets.mdi_history.see(history_size - 1)
                        if history_size > (mdi_history_max_entries + 1):
                            widgets.mdi_history.delete(0, 0)
                            history_size= (mdi_history_max_entries + 1)
                        mdi_history_index = widgets.mdi_history.index("end") - 1
            commands.mdi_history_write_to_file(mdi_history_save_filename, history_size)
            return
        except Tkinter.TclError:
            print "DBG: Sorry, but the clipboard is empty ..."

    def mdi_history_double_butt_1(event):
        if widgets.mdi_command.cget("state") == "disabled":
            return
        cursel= widgets.mdi_history.index("active")
        if cursel < (widgets.mdi_history.size() - 1):
            commands.send_mdi(event)

    def mdi_history_butt_1(event):
        global mdi_history_index
        if len(widgets.mdi_history.curselection()) > 1:
            # multiple selection: clear mdi entry field and return
            vars.mdi_command.set("")
            return
        cursel = widgets.mdi_history.index('@' + str(event.x) + ',' + str(event.y))
        bbox = widgets.mdi_history.bbox(cursel)
        if bbox and (event.y <= (bbox[1] + bbox[3])) and  (cursel < widgets.mdi_history.size() - 1):
            mdi_history_index = cursel
            vars.mdi_command.set(widgets.mdi_history.get(cursel))
        else:
            widgets.mdi_history.see("end")
            widgets.mdi_history.selection_clear(0, "end")
            vars.mdi_command.set("")
            mdi_history_index = widgets.mdi_history.size()

    def clear_mdi_history(*ignored):
        global mdi_history_index, mdi_history_save_filename
        widgets.mdi_history.delete(0, "end")
        widgets.mdi_history.insert(0, "")
        widgets.mdi_history.see(0)
        widgets.mdi_history.selection_clear(0, 0)
        mdi_history_index = 0
        commands.mdi_history_write_to_file(mdi_history_save_filename, -1)

    def ensure_manual(*event):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        commands.set_joint_mode()

    def ensure_mdi(*event):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MDI)

    def redraw(*ignored):
        o.tkRedraw()

    def toggle_show_rapids(*event):
        ap.putpref("show_rapids", vars.show_rapids.get())
        o.tkRedraw()

    def toggle_show_program(*event):
        ap.putpref("show_program", vars.show_program.get())
        o.tkRedraw()

    def toggle_show_live_plot(*event):
        ap.putpref("show_live_plot", vars.show_live_plot.get())
        o.tkRedraw()

    def toggle_show_tool(*event):
        ap.putpref("show_tool", vars.show_tool.get())
        o.tkRedraw()

    def toggle_show_extents(*event):
        ap.putpref("show_extents", vars.show_extents.get())
        o.tkRedraw()

    def toggle_show_machine_limits(*event):
        ap.putpref("show_machine_limits", vars.show_machine_limits.get())
        o.tkRedraw()

    def toggle_show_machine_speed(*event):
        ap.putpref("show_machine_speed", vars.show_machine_speed.get())
        o.tkRedraw()

    def toggle_show_distance_to_go(*event):
        ap.putpref("show_distance_to_go", vars.show_distance_to_go.get())
        o.tkRedraw()

    def toggle_dro_large_font(*event):
        ap.putpref("dro_large_font", vars.dro_large_font.get())
        get_coordinate_font(vars.dro_large_font.get())
        o.tkRedraw()

    def clear_live_plot(*ignored):
        live_plotter.clear()

    # The next three don't have 'manual_ok' because that's done in jog_on /
    # jog_off
    def jog_plus(incr=False):
        a = vars.current_axis.get()
        if isinstance(a, (str, unicode)):
            a = "xyzabcuvw".index(a)
        speed = get_jog_speed(a)
        jog_on(a, speed)
    def jog_minus(incr=False):
        a = vars.current_axis.get()
        if isinstance(a, (str, unicode)):
            a = "xyzabcuvw".index(a)
        speed = get_jog_speed(a)
        jog_on(a, -speed)
    def jog_stop(event=None):
        jog_off(vars.current_axis.get())

    def home_all_axes(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.home(-1)

    def unhome_all_axes(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.unhome(-1)

    def home_axis(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.home("xyzabcuvw".index(vars.current_axis.get()))

    def unhome_axis(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.unhome("xyzabcuvw".index(vars.current_axis.get()))

    def home_axis_number(num):
        ensure_mode(emc.MODE_MANUAL)
        c.home(num)

    def unhome_axis_number(num):
        ensure_mode(emc.MODE_MANUAL)
        c.unhome(num)

    def clear_offset(num):
        ensure_mode(emc.MODE_MDI)
        s.poll()
        if num == "G92":
            clear_command = "G92.1"
        else:
            clear_command = "G10 L2 P%c R0" % num
            for i, a in enumerate("XYZABCUVW"):
                if s.axis_mask & (1<<i): clear_command += " %c0" % a
        c.mdi(clear_command)
        c.wait_complete()
        ensure_mode(emc.MODE_MANUAL)
        s.poll()
        o.tkRedraw()
        reload_file(False)
        
    def touch_off(event=None, new_axis_value = None):
        global system
        if not manual_ok(): return
        if joints_mode(): return
        offset_axis = "xyzabcuvw".index(vars.current_axis.get())
        if new_axis_value is None:
            new_axis_value, system = prompt_touchoff(_("Touch Off"),
                _("Enter %s coordinate relative to workpiece:")
                        % vars.current_axis.get().upper(), 0.0, vars.touch_off_system.get())
        else:
            system = vars.touch_off_system.get()
        if new_axis_value is None: return
        vars.touch_off_system.set(system)
        ensure_mode(emc.MODE_MDI)
        s.poll()

        linear_axis = vars.current_axis.get() in "xyzuvw"
        if linear_axis and vars.metric.get(): scale = 1/25.4
        else: scale = 1

        if linear_axis and 210 in s.gcodes:
            scale *= 25.4

        if system.split()[0] == "T":
            offset_command = "G10 L10 P%d %c[%s*%.12f]" % (s.tool_in_spindle, vars.current_axis.get(), new_axis_value, scale)
            c.mdi(offset_command)
            c.wait_complete()
            c.mdi("G43")
            c.wait_complete()
        else:
            offset_command = "G10 L20 %s %c[%s*%.12f]" % (system.split()[0], vars.current_axis.get(), new_axis_value, scale)
            c.mdi(offset_command)
            c.wait_complete()

        ensure_mode(emc.MODE_MANUAL)
        s.poll()
        o.tkRedraw()
        reload_file(False)

    def set_axis_offset(event=None):
        commands.touch_off(new_axis_value=0.)

    def brake(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.brake(vars.brake.get())
    def flood(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.flood(vars.flood.get())
    def mist(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.mist(vars.mist.get())
    def spindle(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.spindle(vars.spindledir.get())
    def spindle_increase(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.spindle(emc.SPINDLE_INCREASE)
    def spindle_decrease(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.spindle(emc.SPINDLE_DECREASE)
    def spindle_constant(event=None):
        if not manual_ok(): return
        ensure_mode(emc.MODE_MANUAL)
        c.spindle(emc.SPINDLE_CONSTANT)
    def set_first_line(lineno):
        if not manual_ok(): return
        set_first_line(lineno)

    def mist_toggle(*args):
        if not manual_ok(): return
        s.poll()
        c.mist(not s.mist)
    def flood_toggle(*args):
        if not manual_ok(): return
        s.poll()
        c.flood(not s.flood)

    def spindle_forward_toggle(*args):
        if not manual_ok(): return
        s.poll()
        if s.spindle_direction == 0:
            c.spindle(1)
        else:
            c.spindle(0)

    def spindle_backward_toggle(*args):
        if not manual_ok(): return "break"
        s.poll()
        if s.spindle_direction == 0:
            c.spindle(-1)
        else:
            c.spindle(0)
        return "break" # bound to F10, don't activate menu

    def brake_on(*args):
        if not manual_ok(): return
        c.brake(1)
    def brake_off(*args):
        if not manual_ok(): return
        c.brake(0)

    def toggle_display_type(*args):
        vars.display_type.set(not vars.display_type.get())
        o.tkRedraw()

    def toggle_joint_mode(*args):
        vars.joint_mode.set(not vars.joint_mode.get())
        commands.set_joint_mode()

    def toggle_coord_type(*args):
        vars.coord_type.set(not vars.coord_type.get())
        o.tkRedraw()

    def toggle_override_limits(*args):
        s.poll()
        if s.interp_state != emc.INTERP_IDLE: return
        if s.joint[0]['override_limits']:
            ensure_mode(emc.MODE_AUTO)
        else:
            ensure_mode(emc.MODE_MANUAL)
            c.override_limits()

    def cycle_view(*args):
        if str(widgets.view_x['relief']) == "sunken":
            commands.set_view_y()
        elif str(widgets.view_y['relief']) == "sunken":
            commands.set_view_p()
        elif str(widgets.view_z['relief']) == "sunken":
            commands.set_view_z2()
        elif str(widgets.view_z2['relief']) == "sunken":
            commands.set_view_x()
        else:
            commands.set_view_z()

    def axis_activated(*args):
        if not hal_present: return # this only makes sense if HAL is present on this machine
        comp['jog.x'] = vars.current_axis.get() == "x"
        comp['jog.y'] = vars.current_axis.get() == "y"
        comp['jog.z'] = vars.current_axis.get() == "z"
        comp['jog.a'] = vars.current_axis.get() == "a"
        comp['jog.b'] = vars.current_axis.get() == "b"
        comp['jog.c'] = vars.current_axis.get() == "c"
        comp['jog.u'] = vars.current_axis.get() == "u"
        comp['jog.v'] = vars.current_axis.get() == "v"
        comp['jog.w'] = vars.current_axis.get() == "w"

    def set_joint_mode(*args):
        joint_mode = vars.joint_mode.get()
        c.teleop_enable(joint_mode)
        c.wait_complete()

    def save_gcode(*args):
        if not loaded_file: return
        global open_directory
        f = root_window.tk.call("tk_getSaveFile", "-initialdir", open_directory,
            "-filetypes", ((_("rs274ngc files"), ".ngc"),))
        if not f: return
        f = unicode(f)
        open_directory = os.path.dirname(f)
        if get_filter(loaded_file):
            srcfile = os.path.join(tempdir, os.path.basename(loaded_file))
        else:
            srcfile = loaded_file
        try:
            shutil.copy(srcfile, f)
        except (shutil.Error, os.error, IOError), detail:
            tb = traceback.format_exc()
            root_window.tk.call("nf_dialog", ".error", _("Error saving file"),
                str(detail), "error", 0, _("OK"))
        else:
            add_recent_file(f)

commands = TclCommands(root_window)

vars = nf.Variables(root_window, 
    ("emctop_command", StringVar),
    ("emcini", StringVar),
    ("mdi_command", StringVar),
    ("taskfile", StringVar),
    ("interp_pause", IntVar),
    ("exec_state", IntVar),
    ("task_state", IntVar),
    ("task_paused", IntVar),
    ("interp_state", IntVar),
    ("task_mode", IntVar),
    ("has_ladder", IntVar),
    ("has_editor", IntVar),
    ("current_axis", StringVar),
    ("mist", BooleanVar),
    ("flood", BooleanVar),
    ("brake", BooleanVar),
    ("spindledir", IntVar),
    ("running_line", IntVar),
    ("highlight_line", IntVar),
    ("show_program", IntVar),
    ("show_live_plot", IntVar),
    ("show_tool", IntVar),
    ("show_extents", IntVar),
    ("show_machine_limits", IntVar),
    ("show_machine_speed", IntVar),
    ("show_distance_to_go", IntVar),
    ("dro_large_font", IntVar),
    ("show_rapids", IntVar),
    ("feedrate", IntVar),
    ("spindlerate", IntVar),
    ("tool", StringVar),
    ("active_codes", StringVar),
    ("metric", IntVar),
    ("coord_type", IntVar),
    ("display_type", IntVar),
    ("override_limits", BooleanVar),
    ("view_type", IntVar),
    ("jog_speed", DoubleVar),
    ("jog_aspeed", DoubleVar),
    ("max_speed", DoubleVar),
    ("max_aspeed", DoubleVar),
    ("maxvel_speed", DoubleVar),
    ("max_maxvel", DoubleVar),
    ("joint_mode", IntVar),
    ("motion_mode", IntVar),
    ("kinematics_type", IntVar),
    ("optional_stop", BooleanVar),
    ("block_delete", BooleanVar),
    ("rotate_mode", BooleanVar),
    ("touch_off_system", StringVar),
    ("machine", StringVar),
    ("on_any_limit", BooleanVar),
)
vars.emctop_command.set(os.path.join(os.path.dirname(sys.argv[0]), "emctop"))
vars.highlight_line.set(-1)
vars.running_line.set(-1)
vars.show_program.set(ap.getpref("show_program", True))
vars.show_rapids.set(ap.getpref("show_rapids", True))
vars.show_live_plot.set(ap.getpref("show_live_plot", True))
vars.show_tool.set(ap.getpref("show_tool", True))
vars.show_extents.set(ap.getpref("show_extents", True))
vars.show_machine_limits.set(ap.getpref("show_machine_limits", True))
vars.show_machine_speed.set(ap.getpref("show_machine_speed", True))
vars.show_distance_to_go.set(ap.getpref("show_distance_to_go", False))
vars.dro_large_font.set(ap.getpref("dro_large_font", False))
vars.block_delete.set(ap.getpref("block_delete", True))
vars.optional_stop.set(ap.getpref("optional_stop", True))

# placeholder function for LivePlotter.update():
def user_live_update():
    pass

vars.touch_off_system.set("P1  G54")

update_recent_menu()

def set_feedrate(n):
    widgets.feedoverride.set(n)

def activate_axis_or_set_feedrate(n):
    # XXX: axis_mask does not apply if in joint mode
    if manual_ok() and s.axis_mask & (1<<n):
        activate_axis(n)
    else:
        set_feedrate(10*n)

def nomodifier(f):
    def g(event):
        if event.state & (1|4|8|32|64|128): return ""
        return f(event)
    return g

def kp_wrap(f,g):
    return nomodifier(f)

root_window.bind("<Escape>", commands.task_stop)
root_window.bind("l", commands.toggle_override_limits)
root_window.bind("o", commands.open_file)
root_window.bind("s", commands.task_resume)
root_window.bind("t", commands.task_step)
root_window.bind("p", commands.task_pause)
root_window.bind("v", commands.cycle_view)
root_window.bind("<Alt-p>", "#nothing")
root_window.bind("r", commands.task_run)
root_window.bind("<Control-r>", commands.reload_file)
root_window.bind("<Control-s>", commands.save_gcode)
root_window.bind_class("all", "<Key-F1>", commands.estop_clicked)
root_window.bind("<Key-F2>", commands.onoff_clicked)
root_window.bind("<Key-F7>", commands.mist_toggle)
root_window.bind("<Key-F8>", commands.flood_toggle)
root_window.bind("<Key-F9>", commands.spindle_forward_toggle)
root_window.bind("<Key-F10>", commands.spindle_backward_toggle)
root_window.bind("<Key-F11>", commands.spindle_decrease)
root_window.bind("<Key-F12>", commands.spindle_increase)
root_window.bind("B", commands.brake_on)
root_window.bind("b", commands.brake_off)
root_window.bind("<Control-k>", commands.clear_live_plot)
root_window.bind("x", lambda event: activate_axis(0))
root_window.bind("y", lambda event: activate_axis(1))
root_window.bind("z", lambda event: activate_axis(2))
root_window.bind("a", lambda event: activate_axis(3))
root_window.bind("`", lambda event: activate_axis_or_set_feedrate(0))
root_window.bind("1", lambda event: activate_axis_or_set_feedrate(1))
root_window.bind("2", lambda event: activate_axis_or_set_feedrate(2))
root_window.bind("3", lambda event: activate_axis_or_set_feedrate(3))
root_window.bind("4", lambda event: activate_axis_or_set_feedrate(4))
root_window.bind("5", lambda event: activate_axis_or_set_feedrate(5))
root_window.bind("6", lambda event: activate_axis_or_set_feedrate(6))
root_window.bind("7", lambda event: activate_axis_or_set_feedrate(7))
root_window.bind("8", lambda event: activate_axis_or_set_feedrate(8))
root_window.bind("9", lambda event: set_feedrate(90))
root_window.bind("0", lambda event: set_feedrate(100))
root_window.bind("c", lambda event: jogspeed_continuous())
root_window.bind("d", lambda event: widgets.rotate.invoke())
root_window.bind("i", lambda event: jogspeed_incremental())
root_window.bind("I", lambda event: jogspeed_incremental(-1))
root_window.bind("!", "set metric [expr {!$metric}]; redraw")
root_window.bind("@", commands.toggle_display_type)
root_window.bind("#", commands.toggle_coord_type)
root_window.bind("$", commands.toggle_joint_mode)

root_window.bind("<Home>", commands.home_axis)
root_window.bind("<KP_Home>", kp_wrap(commands.home_axis, "KeyPress"))
root_window.bind("<Control-Home>", commands.home_all_axes)
root_window.bind("<Shift-Home>", commands.set_axis_offset)
root_window.bind("<End>", commands.touch_off)
root_window.bind("<Control-KP_Home>", kp_wrap(commands.home_all_axes, "KeyPress"))
root_window.bind("<Shift-KP_Home>", kp_wrap(commands.set_axis_offset, "KeyPress"))
root_window.bind("<KP_End>", kp_wrap(commands.touch_off, "KeyPress"))
widgets.mdi_history.bind("<Configure>", "%W see end" )
widgets.mdi_history.bind("<ButtonRelease-1>", commands.mdi_history_butt_1)
widgets.mdi_history.bind("<Double-Button-1>", commands.mdi_history_double_butt_1)
widgets.mdi_command.unbind("<Control-h>")
widgets.mdi_command.bind("<Control-m>", commands.clear_mdi_history)
widgets.mdi_command.bind("<Control-h>", commands.mdi_history_hist2clip)
widgets.mdi_command.bind("<Control-Shift-H>", commands.mdi_history_clip2hist)
widgets.mdi_command.bind("<Key-Return>", commands.send_mdi)
widgets.mdi_command.bind("<Up>",  commands.mdi_up_cmd)
widgets.mdi_command.bind("<Down>", commands.mdi_down_cmd)
widgets.mdi_command.bind("<Key-KP_Enter>", commands.send_mdi)
widgets.mdi_command.bind("<KP_Up>",  commands.mdi_up_cmd)
widgets.mdi_command.bind("<KP_Down>", commands.mdi_down_cmd)
widgets.mdi_command.bind("<KeyRelease-minus>", "break")
widgets.mdi_command.bind("<KeyRelease-equal>", "break")

# try to read back previously saved mdi history data
mdi_hist_file = os.path.expanduser(mdi_history_save_filename)
try:
    line_cnt = 0
    f = open(mdi_hist_file, "r")
    try:
        for line in f:
            line_cnt += 1
    finally:
        f.seek(0)
    skip = line_cnt - mdi_history_max_entries
    history_size = 1
    try:
        for line in f:
            if skip <= 0:
                widgets.mdi_history.insert(history_size - 1, "%s" % line.rstrip("\r\n"))
                mdi_history_index = history_size
                history_size += 1
            else:
                skip -= 1
    finally:    
        f.close()
except IOError:
    pass



def jog(*args):
    if not manual_ok(): return
    ensure_mode(emc.MODE_MANUAL)
    c.jog(*args)

# XXX correct for machines with more than six axes
jog_after = [None] * 9
jog_cont  = [False] * 9
jogging   = [0] * 9
def jog_on(a, b):
    if not manual_ok(): return
    if isinstance(a, (str, unicode)):
        a = "xyzabcuvw".index(a)
    if a < 3:
        if vars.metric.get(): b = b / 25.4
        b = from_internal_linear_unit(b)
    if jog_after[a]:
        root_window.after_cancel(jog_after[a])
        jog_after[a] = None
        return
    jogincr = widgets.jogincr.get()
    if s.motion_mode == emc.TRAJ_MODE_TELEOP:
        jogging[a] = b
        jog_cont[a] = False
        cartesian_only=jogging[:6]
        c.teleop_vector(*cartesian_only)
    else:
        if jogincr != _("Continuous"):
            s.poll()
            if s.state != 1: return
            distance = parse_increment(jogincr)
            jog(emc.JOG_INCREMENT, a, b, distance)
            jog_cont[a] = False
        else:
            jog(emc.JOG_CONTINUOUS, a, b)
            jog_cont[a] = True
            jogging[a] = b

def jog_off(a):
    if isinstance(a, (str, unicode)):
        a = "xyzabcuvw".index(a)
    if jog_after[a]: return
    jog_after[a] = root_window.after_idle(lambda: jog_off_actual(a))

def jog_off_actual(a):
    if not manual_ok(): return
    activate_axis(a)
    jog_after[a] = None
    jogging[a] = 0
    if s.motion_mode == emc.TRAJ_MODE_TELEOP:
        cartesian_only=jogging[:6]
        c.teleop_vector(*cartesian_only)
    else:
        if jog_cont[a]:
            jog(emc.JOG_STOP, a)

def jog_off_all():
    for i in range(6):
        if jogging[i]:
            jog_off_actual(i)

def bind_axis(a, b, d):
    root_window.bind("<KeyPress-%s>" % a, kp_wrap(lambda e: jog_on(d, -get_jog_speed(d)), "KeyPress"))
    root_window.bind("<KeyPress-%s>" % b, kp_wrap(lambda e: jog_on(d, get_jog_speed(d)), "KeyPress"))
    root_window.bind("<Shift-KeyPress-%s>" % a, lambda e: jog_on(d, -get_max_jog_speed(d)))
    root_window.bind("<Shift-KeyPress-%s>" % b, lambda e: jog_on(d, get_max_jog_speed(d)))
    root_window.bind("<KeyRelease-%s>" % a, lambda e: jog_off(d))
    root_window.bind("<KeyRelease-%s>" % b, lambda e: jog_off(d))

root_window.bind("<FocusOut>", lambda e: str(e.widget) == "." and jog_off_all())

import sys, getopt

open_directory = "programs"

unit_values = {'inch': 1/25.4, 'mm': 1}
def units(s, d=1.0):
    try:
        return float(s)
    except ValueError:
        return unit_values.get(s, d)

if sys.argv[1] != "-ini":
    raise SystemExit, "-ini must be first argument"

inifile = emc.ini(sys.argv[2])
vars.emcini.set(sys.argv[2])
jointcount = int(inifile.find("KINS", "JOINTS"))
jointnames = "012345678"[:jointcount]
open_directory = inifile.find("DISPLAY", "PROGRAM_PREFIX")
vars.machine.set(inifile.find("EMC", "MACHINE"))
extensions = inifile.findall("FILTER", "PROGRAM_EXTENSION")
extensions = [e.split(None, 1) for e in extensions]
extensions = tuple([(v, tuple(k.split(","))) for k, v in extensions])
postgui_halfile = inifile.find("HAL", "POSTGUI_HALFILE")
max_feed_override = float(inifile.find("DISPLAY", "MAX_FEED_OVERRIDE"))
max_spindle_override = float(inifile.find("DISPLAY", "MAX_SPINDLE_OVERRIDE") or max_feed_override)
max_feed_override = int(max_feed_override * 100 + 0.5)
max_spindle_override = int(max_spindle_override * 100 + 0.5)

geometry = inifile.find("DISPLAY", "GEOMETRY") or "XYZBCUVW"
geometry = re.split(" *(-?[XYZABCUVW])", geometry.upper())
geometry = "".join(reversed(geometry))

jog_speed = (
    inifile.find("DISPLAY", "DEFAULT_LINEAR_VELOCITY")
    or inifile.find("TRAJ", "DEFAULT_LINEAR_VELOCITY")
    or inifile.find("TRAJ", "DEFAULT_VELOCITY")
    or 1.0)
vars.jog_speed.set(float(jog_speed)*60)
jog_speed = (
    inifile.find("DISPLAY", "DEFAULT_ANGULAR_VELOCITY")
    or inifile.find("TRAJ", "DEFAULT_ANGULAR_VELOCITY")
    or inifile.find("TRAJ", "DEFAULT_VELOCITY")
    or jog_speed)
vars.jog_aspeed.set(float(jog_speed)*60)
mlv = (
    inifile.find("DISPLAY","MAX_LINEAR_VELOCITY")
    or inifile.find("TRAJ","MAX_LINEAR_VELOCITY")
    or inifile.find("TRAJ","MAX_VELOCITY")
    or 1.0)
vars.max_speed.set(float(mlv))
mav = (
    inifile.find("DISPLAY","MAX_ANGULAR_VELOCITY")
    or inifile.find("TRAJ","MAX_ANGULAR_VELOCITY")
    or inifile.find("TRAJ","MAX_VELOCITY")
    or mlv)
vars.max_aspeed.set(float(mav))
mv = inifile.find("TRAJ","MAX_LINEAR_VELOCITY") or inifile.find("AXIS_0","MAX_VELOCITY") or 1.0
vars.maxvel_speed.set(float(mv)*60)
vars.max_maxvel.set(float(mv))
root_window.tk.eval("${pane_top}.jogspeed.s set [setval $jog_speed $max_speed]")
root_window.tk.eval("${pane_top}.ajogspeed.s set [setval $jog_aspeed $max_aspeed]")
root_window.tk.eval("${pane_top}.maxvel.s set [setval $maxvel_speed $max_maxvel]")
widgets.feedoverride.configure(to=max_feed_override)
widgets.spinoverride.configure(to=max_spindle_override)
nmlfile = inifile.find("EMC", "NML_FILE")
if nmlfile:
    emc.nmlfile = os.path.join(os.path.dirname(sys.argv[2]), nmlfile)
vars.coord_type.set(inifile.find("DISPLAY", "POSITION_OFFSET") == "RELATIVE")
vars.display_type.set(inifile.find("DISPLAY", "POSITION_FEEDBACK") == "COMMANDED")
coordinate_display = inifile.find("DISPLAY", "POSITION_UNITS")
lathe = bool(inifile.find("DISPLAY", "LATHE"))
editor = inifile.find("DISPLAY", "EDITOR")
vars.has_editor.set(editor is not None)
tooleditor = inifile.find("DISPLAY", "TOOL_EDITOR") or "tooledit"
tooltable = inifile.find("EMCIO", "TOOL_TABLE")
lu = units(inifile.find("TRAJ", "LINEAR_UNITS"))
a_axis_wrapped = inifile.find("AXIS_3", "WRAPPED_ROTARY")
b_axis_wrapped = inifile.find("AXIS_4", "WRAPPED_ROTARY")
c_axis_wrapped = inifile.find("AXIS_5", "WRAPPED_ROTARY")
if coordinate_display:
    if coordinate_display.lower() in ("mm", "metric"): vars.metric.set(1)
    else: vars.metric.set(0)
else:
    if lu in [.001, .01, .1, 1, 10]: vars.metric.set(1)
    else: vars.metric.set(0)
if lu == 1:
    root_window.tk.eval("${pane_top}.jogspeed.l1 configure -text mm/min")
    root_window.tk.eval("${pane_top}.maxvel.l1 configure -text mm/min")
else:
    root_window.tk.eval("${pane_top}.jogspeed.l1 configure -text in/min")
    root_window.tk.eval("${pane_top}.maxvel.l1 configure -text in/min")
root_window.tk.eval(u"${pane_top}.ajogspeed.l1 configure -text deg/min")
homing_order_defined = inifile.find("JOINT_0", "HOME_SEQUENCE") is not None

if homing_order_defined:
    widgets.homebutton.configure(text=_("Home All"), command="home_all_axes")
    root_window.tk.call("DynamicHelp::add", widgets.homebutton,
            "-text", _("Home all axes [Ctrl-Home]"))
    widgets.homemenu.add_command(command=commands.home_all_axes)
    root_window.tk.call("setup_menu_accel", widgets.homemenu, "end",
            _("Home All Axes"))

update_ms = int(1000 * float(inifile.find("DISPLAY","CYCLE_TIME") or 0.020))

widgets.unhomemenu.add_command(command=commands.unhome_all_axes)
root_window.tk.call("setup_menu_accel", widgets.unhomemenu, "end", _("Unhome All Axes"))

s = emc.stat();
s.poll()
statfail=0
while s.axes == 0:
    print "waiting for s.axes"
    time.sleep(.01)
    statfail+=1
    if statfail > 500:
        raise SystemExit, "Invalid configuration of axes is preventing EMC from starting"
    s.poll()

num_joints = s.joints
for i in range(num_joints):
    widgets.homemenu.add_command(command=lambda i=i: commands.home_axis_number(i))
    widgets.unhomemenu.add_command(command=lambda i=i: commands.unhome_axis_number(i))
    root_window.tk.call("setup_menu_accel", widgets.homemenu, "end",
            _("Home Joint _%s") % i)
    root_window.tk.call("setup_menu_accel", widgets.unhomemenu, "end",
            _("Unhome Joint _%s") % i)

astep_size = step_size = 1
for a in range(9):
    if s.axis_mask & (1<<a) == 0: continue
    section = "AXIS_%d" % a
    unit = inifile.find(section, "UNITS") or lu
    unit = units(unit) * 25.4
    f = inifile.find(section, "SCALE") or inifile.find(section, "INPUT_SCALE") or "8000"
    try:
        f = abs(float(f.split()[0]))
    except ValueError:
        pass
    else:
        if f != 0:
            step_size_tmp = min(step_size, 1. / f)
            if a < 3: step_size = astep_size = step_size_tmp
            else: astep_size = step_size_tmp

if inifile.find("DISPLAY", "MIN_LINEAR_VELOCITY"):
    root_window.tk.call("set_slider_min", float(inifile.find("DISPLAY", "MIN_LINEAR_VELOCITY"))*60)
elif inifile.find("DISPLAY", "MIN_VELOCITY"):
    root_window.tk.call("set_slider_min", float(inifile.find("DISPLAY", "MIN_VELOCITY"))*60)
elif step_size != 1:
    root_window.tk.call("set_slider_min", step_size*30)
if inifile.find("DISPLAY", "MIN_ANGULAR_VELOCITY"):
    root_window.tk.call("set_aslider_min", float(inifile.find("DISPLAY", "MIN_ANGULAR_VELOCITY"))*60)
elif inifile.find("DISPLAY", "MIN_VELOCITY"):
    root_window.tk.call("set_aslider_min", float(inifile.find("DISPLAY", "MIN_VELOCITY"))*60)
elif astep_size != 1:
    root_window.tk.call("set_aslider_min", astep_size*30)

increments = inifile.find("DISPLAY", "INCREMENTS")
if increments:
    if "," in increments:
        increments = [i.strip() for i in increments.split(",")]
    else:
        increments = increments.split()
    root_window.call(widgets.jogincr._w, "list", "delete", "1", "end")
    root_window.call(widgets.jogincr._w, "list", "insert", "end", *increments)
widgets.jogincr.configure(command= jogspeed_listbox_change)
root_window.call(widgets.jogincr._w, "select", 0)   

vcp = inifile.find("DISPLAY", "PYVCP")

del sys.argv[1:3]

root_window.bind("<KeyPress-KP_Begin>", kp_wrap(lambda e: None, "KeyPress"))
root_window.bind("<KeyPress-KP_Insert>", kp_wrap(lambda e: None, "KeyPress"))

if lathe:
    bind_axis("Left", "Right", 2)
    bind_axis("Up", "Down", 0)
    bind_axis("KP_Left", "KP_Right", 2)
    bind_axis("KP_Up", "KP_Down", 0)
    root_window.bind("<KeyPress-KP_Next>", kp_wrap(lambda e: None, "KeyPress"))
    root_window.bind("<KeyPress-KP_Prior>", kp_wrap(lambda e: None, "KeyPress"))
else:
    bind_axis("Left", "Right", 0)
    bind_axis("Down", "Up", 1)
    bind_axis("Next", "Prior", 2)
    bind_axis("KP_Left", "KP_Right", 0)
    bind_axis("KP_Down", "KP_Up", 1)
    bind_axis("KP_Next", "KP_Prior", 2)
    bind_axis("bracketleft", "bracketright", 3)

root_window.bind("<KeyPress-minus>", nomodifier(commands.jog_minus))
root_window.bind("<KeyPress-equal>", nomodifier(commands.jog_plus))
root_window.bind("<KeyRelease-minus>", commands.jog_stop)
root_window.bind("<KeyRelease-equal>", commands.jog_stop)



opts, args = getopt.getopt(sys.argv[1:], 'd:')
for i in range(9):
    if s.axis_mask & (1<<i): continue
    c = getattr(widgets, "axis_%s" % ("xyzabcuvw"[i]))
    c.grid_forget()
for i in range(num_joints, 9):
    c = getattr(widgets, "joint_%d" % i)
    c.grid_forget()
    
if s.axis_mask & 56 == 0:
    widgets.ajogspeed.grid_forget()
c = emc.command()
e = emc.error_channel()

c.set_block_delete(vars.block_delete.get())
c.wait_complete()
c.set_optional_stop(vars.optional_stop.get())
c.wait_complete()

o = MyOpengl(widgets.preview_frame, width=400, height=300, double=1, depth=1)
o.last_line = 1
o.pack(fill="both", expand=1)


# Find font for coordinate readout and get metrics
font_cache = {}
def get_coordinate_font(large):
    global coordinate_font
    global coordinate_linespace
    global coordinate_charwidth
    global fontbase

    if large:
        coordinate_font = "-*-lucidatypewriter-medium-r-*-*-20-*-*-*-*-*-*-1"
    else:
        coordinate_font = "9x15"
    coordinate_charwidth, coordinate_linespace = map(int,
        o.tk.call(o, "measurebitmapfont", coordinate_font).split())
    
    if coordinate_font not in font_cache:
        font_cache[coordinate_font] = fontbase = int(o.tk.call(o._w, "loadbitmapfont", coordinate_font))
    else:
        fontbase = font_cache[coordinate_font]

root_window.bind("<Key-F3>", pane_top + ".tabs raise manual")
root_window.bind("<Key-F5>", pane_top + ".tabs raise mdi")
root_window.bind("<Key-F5>", "+" + tabs_mdi + ".command selection range 0 end")
root_window.bind("<Key-F4>", commands.toggle_preview)

init()

#right click menu for the program
def rClicker(e):
    
    def select_run_from(e):
        commands.task_run_line()

    #if no line is selected drop out
    if vars.highlight_line.get() == -1 :
        return
    nclst=[
        ('        ',None),   #
        (' ------ ',None),   #
        (_('Run from here'), lambda e=e: select_run_from(e)),
        ]
    rmenu = Tkinter.Menu(None, tearoff=0, takefocus=0)
    cas = {}
    for (txt, cmd) in nclst:
        if txt == ' ------ ':
            rmenu.add_separator()
        else: rmenu.add_command(label=txt, command=cmd)
    rmenu.entryconfigure(0, label = "AXIS", state = 'disabled')
    rmenu.tk_popup(e.x_root-3, e.y_root+3,entry="0")
    return "break"

t = widgets.text
t.bind('<Button-3>', rClicker) #allow right-click to select start from line
t.tag_configure("ignored", background="#ffffff", foreground="#808080")
t.tag_configure("lineno", foreground="#808080")
t.tag_configure("executing", background="#804040", foreground="#ffffff")
if args:
    for i, l in enumerate(open(args[0])):
        l = l.expandtabs().replace("\r", "")
        t.insert("end", "%6d: " % (i+1), "lineno", l)
t.bind("<Button-1>", select_line)
t.bind("<B1-Motion>", lambda e: "break")
t.bind("<B1-Leave>", lambda e: "break")
t.bind("<Button-4>", scroll_up)
t.bind("<Button-5>", scroll_down)
t.configure(state="disabled")

if hal_present == 1 :
    comp = hal.component("axisui")
    comp.newpin("jog.x", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.y", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.z", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.a", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.b", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.c", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.u", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.v", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.w", hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin("jog.increment", hal.HAL_FLOAT, hal.HAL_OUT)
    comp.newpin("set-manual-mode",hal.HAL_BIT,hal.HAL_IN)
    comp.newpin("notifications-clear",hal.HAL_BIT,hal.HAL_IN)
    comp.newpin("notifications-clear-info",hal.HAL_BIT,hal.HAL_IN)
    comp.newpin("notifications-clear-error",hal.HAL_BIT,hal.HAL_IN)
    vars.has_ladder.set(hal.component_exists('classicladder_rt'))

    if vcp:
        import vcpparse
        comp.setprefix("pyvcp")
        f = Tkinter.Frame(root_window)
        f.grid(row=0, column=4, rowspan=6, sticky="nw", padx=4, pady=4)
        vcpparse.filename = vcp
        vcpparse.create_vcp(f, comp)
    comp.ready()

notifications = Notification(root_window)

root_window.bind("<Control-space>", lambda event: notifications.clear())
widgets.mdi_command.bind("<Control-space>", lambda event: notifications.clear())


make_cone()

get_coordinate_font(vars.dro_large_font.get())

live_plotter = LivePlotter(o)
live_plotter.start()
hershey = Hershey()

def color_limit(cond):
    if cond:
        glColor3f(*o.colors['label_limit'])
    else:
        glColor3f(*o.colors['label_ok'])
    return cond

def remove_tempdir(t):
    shutil.rmtree(t)
tempdir = tempfile.mkdtemp()
atexit.register(remove_tempdir, tempdir)

if postgui_halfile:
    res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i", vars.emcini.get(), "-f", postgui_halfile])
    if res: raise SystemExit, res

activate_axis(0, True)
set_hal_jogincrement()

code = []
addrecent = True
if args:
    initialfile = args[0]
elif os.environ.has_key("AXIS_OPEN_FILE"):
    initialfile = os.environ["AXIS_OPEN_FILE"]
elif inifile.find("DISPLAY", "OPEN_FILE"):
    initialfile = inifile.find("DISPLAY", "OPEN_FILE")
elif lathe:
    initialfile = os.path.join(BASE, "share", "axis", "images","axis-lathe.ngc")
    addrecent = False
else:
    initialfile = os.path.join(BASE, "share", "axis", "images", "axis.ngc")
    addrecent = False

if os.path.exists(initialfile):
    open_file_guts(initialfile, False, addrecent)

if lathe:
    commands.set_view_y()
else:
    commands.set_view_p()
if o.g:
    x = (o.g.min_extents[0] + o.g.max_extents[0])/2
    y = (o.g.min_extents[1] + o.g.max_extents[1])/2
    z = (o.g.min_extents[2] + o.g.max_extents[2])/2
    o.set_centerpoint(x, y, z)

try:
    root_window.tk.call("send", "-async", "popimage", "destroy .")
except Tkinter.TclError:
    pass

o.update_idletasks()

import _tk_seticon
from rs274.icon import icon
_tk_seticon.seticon(root_window, icon)
_tk_seticon.seticon(widgets.about_window, icon)
_tk_seticon.seticon(widgets.help_window, icon)

vars.kinematics_type.set(s.kinematics_type)
def balance_ja():
    w = max(widgets.axes.winfo_reqwidth(), widgets.joints.winfo_reqwidth())
    h = max(widgets.axes.winfo_reqheight(), widgets.joints.winfo_reqheight())
    widgets.axes.configure(width=w, height=h)
    widgets.joints.configure(width=w, height=h)
if s.kinematics_type != emc.KINEMATICS_IDENTITY:
    c.teleop_enable(0)
    c.wait_complete()
    vars.joint_mode.set(0)
    widgets.joints.grid_propagate(0)
    widgets.axes.grid_propagate(0)
    root_window.after_idle(balance_ja)
else:
    widgets.menu_view.delete("end")
    widgets.menu_view.delete("end")
    widgets.menu_view.delete("end")
    root_window.bind("$", "")


if lathe:
    root_window.after_idle(commands.set_view_y)
    root_window.bind("v", commands.set_view_y)
    root_window.bind("d", "")
    widgets.view_z.pack_forget()
    widgets.view_z2.pack_forget()
    widgets.view_x.pack_forget()
    widgets.view_y.pack_forget()
    widgets.view_p.pack_forget()
    widgets.rotate.pack_forget()
    widgets.axis_y.grid_forget()
    widgets.menu_view.delete(0, 5)

widgets.feedoverride.set(100)
commands.set_feedrate(100)
widgets.spinoverride.set(100)
commands.set_spindlerate(100)

def forget(widget, *pins):
    if os.environ.has_key("AXIS_NO_AUTOCONFIGURE"): return
    if hal_present == 1 :
        for p in pins:
            if hal.pin_has_writer(p): return
    m = widget.winfo_manager()
    if m in ("grid", "pack"):
        widget.tk.call(m, "forget", widget._w)

forget(widgets.brake, "motion.spindle-brake")
forget(widgets.spindle_cw, "motion.spindle-forward", "motion.spindle-on")
forget(widgets.spindle_ccw, "motion.spindle-reverse")
forget(widgets.spindle_stop, "motion.spindle-forward", "motion.spindle-reverse", "motion.spindle-on")
forget(widgets.spindle_plus, "motion.spindle-speed-out")
forget(widgets.spindle_minus, "motion.spindle-speed-out")
forget(widgets.spindlef,  "motion.spindle-forward", "motion.spindle-reverse",
    "motion.spindle-on", "motion.spindle-speed-out",
    "motion.spindle-brake")
forget(widgets.spindlel,  "motion.spindle-forward", "motion.spindle-reverse",
    "motion.spindle-on", "motion.spindle-speed-out",
    "motion.spindle-brake")
forget(widgets.spinoverridef, "motion.spindle-speed-out")

has_limit_switch = 0
for j in range(9):
    try:
        if hal.pin_has_writer("joint.%d.neg-lim-sw-in" % j):
            has_limit_switch=1
            break
        if hal.pin_has_writer("joint.%d.pos-lim-sw-in" % j):
            has_limit_switch=1
            break
    except NameError, detail:
        break
if not has_limit_switch:
    widgets.override.grid_forget()


forget(widgets.mist, "iocontrol.0.coolant-mist")
forget(widgets.flood, "iocontrol.0.coolant-flood")
forget(widgets.lubel, "iocontrol.0.coolant-flood", "iocontrol.0.coolant-mist")

rcfile = "~/.axisrc"
user_command_file = inifile.find("DISPLAY", "USER_COMMAND_FILE") or ""
if user_command_file:
    rcfile = user_command_file
rcfile = os.path.expanduser(rcfile)
if os.path.exists(rcfile):
    import traceback
    try:
        execfile(rcfile)
    except:
        tb = traceback.format_exc()
        print >>sys.stderr, tb
        root_window.tk.call("nf_dialog", ".error", _("Error in ~/.axisrc"),
            tb, "error", 0, _("OK"))

root_window.tk.call("trace", "variable", "metric", "w", "update_units")
install_help(root_window)

widgets.numbers_text.bind("<Configure>", commands.redraw_soon)
live_plotter.update()
live_plotter.error_task()
o.mainloop()
live_plotter.stop()

# vim:sw=4:sts=4:et:
