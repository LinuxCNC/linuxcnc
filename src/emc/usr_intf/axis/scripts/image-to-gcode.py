#!/usr/bin/env python

## image-to-gcode is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2 of the License, or (at your
## option) any later version.  image-to-gcode is distributed in the hope 
## that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
## warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
## the GNU General Public License for more details.  You should have
## received a copy of the GNU General Public License along with image-to-gcode;
## if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
## Fifth Floor, Boston, MA 02110-1301 USA.
## 
## image-to-gcode.py is Copyright (C) 2005 Chris Radek
## chris@timeguy.com
## image-to-gcode.py is Copyright (C) 2006 Jeff Epler
## jepler@unpy.net

import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import gettext;
gettext.install("linuxcnc", localedir=os.path.join(BASE, "share", "locale"), unicode=True)

try:
    from PIL import Image
except:
    import Image

import numpy.core
plus_inf = numpy.core.Inf

from rs274.author import Gcode
import rs274.options

from math import *
import operator

epsilon = 1e-5

def tobytes(img):
    if hasattr(img, 'tobytes'): return img.tobytes()
    return img.tostring()

def ball_tool(r,rad):
    s = -sqrt(rad**2-r**2)
    return s

def endmill(r,dia):
    return 0

def vee_common(angle):
    slope = tan(angle * pi / 180)
    def f(r, dia):
        return r * slope
    return f

tool_makers = [ ball_tool, endmill, vee_common(30), vee_common(45), vee_common(60)]

def make_tool_shape(f, wdia, resp):
    res = 1. / resp
    dia = int(wdia*res+.5)
    wrad = wdia/2.
    if dia < 2: dia = 2
    n = numpy.array([[plus_inf] * dia] * dia, dtype=numpy.float32)
    hdia = dia / 2.
    l = []
    for x in range(dia):
        for y in range(dia):
            r = hypot(x-hdia, y-hdia) * resp
            if r < wrad:
                z = f(r, wrad)
                l.append(z)
                n[x,y] = z
    n = n - n.min()
    return n

def amax(seq):
    res = 0
    for i in seq:
        if abs(i) > abs(res): res = i
    return res

def group_by_sign(seq, slop=sin(pi/18), key=lambda x:x):
    sign = None
    subseq = []
    for i in seq:
        ki = key(i)
        if sign is None:
            subseq.append(i)
            if ki != 0:
                sign = ki / abs(ki)
        else:
            subseq.append(i)
            if sign * ki < -slop:
                sign = ki / abs(ki)
                yield subseq
                subseq = [i]
    if subseq: yield subseq

class Convert_Scan_Alternating:
    def __init__(self):
        self.st = 0

    def __call__(self, primary, items):
        st = self.st = self.st + 1
        if st % 2: items.reverse()
        if st == 1: yield True, items
        else: yield False, items

    def reset(self):
        self.st = 0

class Convert_Scan_Increasing:
    def __call__(self, primary, items):
        yield True, items

    def reset(self):
        pass

class Convert_Scan_Decreasing:
    def __call__(self, primary, items):
        items.reverse()
        yield True, items

    def reset(self):
        pass

class Convert_Scan_Upmill:
    def __init__(self, slop = sin(pi / 18)):
        self.slop = slop

    def __call__(self, primary, items):
        for span in group_by_sign(items, self.slop, operator.itemgetter(2)):
            if amax([it[2] for it in span]) < 0:
                span.reverse()
            yield True, span

    def reset(self):
        pass

class Convert_Scan_Downmill:
    def __init__(self, slop = sin(pi / 18)):
        self.slop = slop

    def __call__(self, primary, items):
        for span in group_by_sign(items, self.slop, operator.itemgetter(2)):
            if amax([it[2] for it in span]) > 0:
                span.reverse()
            yield True, span

    def reset(self):
        pass

class Reduce_Scan_Lace:
    def __init__(self, converter, slope, keep):
        self.converter = converter
        self.slope = slope
        self.keep = keep

    def __call__(self, primary, items):
        slope = self.slope
        keep = self.keep
        if primary:
            idx = 3
            test = operator.le
        else:
            idx = 2
            test = operator.ge

        def bos(j):
            return j - j % keep

        def eos(j):
            if j % keep == 0: return j
            return j + keep - j%keep

        for i, (flag, span) in enumerate(self.converter(primary, items)):
            subspan = []
            a = None
            for i, si in enumerate(span):
                ki = si[idx]
                if a is None:
                    if test(abs(ki), slope):
                        a = b = i
                else:
                    if test(abs(ki), slope):
                        b = i
                    else:
                        if i - b < keep: continue
                        yield True, span[bos(a):eos(b+1)]
                        a = None
            if a is not None:
                yield True, span[a:]

    def reset(self):
        self.converter.reset()

unitcodes = ['G20', 'G21']
convert_makers = [ Convert_Scan_Increasing, Convert_Scan_Decreasing, Convert_Scan_Alternating, Convert_Scan_Upmill, Convert_Scan_Downmill ]

def progress(a, b):
    if os.environ.has_key("AXIS_PROGRESS_BAR"):
        print >>sys.stderr, "FILTER_PROGRESS=%d" % int(a*100./b+.5)
        sys.stderr.flush()

class Converter:
    def __init__(self,
            image, units, tool_shape, pixelsize, pixelstep, safetyheight, \
            tolerance, feed, convert_rows, convert_cols, cols_first_flag,
            entry_cut, spindle_speed, roughing_offset, roughing_delta,
            roughing_feed):
        self.image = image
        self.units = units
        self.tool = tool_shape
        self.pixelsize = pixelsize
        self.pixelstep = pixelstep
        self.safetyheight = safetyheight
        self.tolerance = tolerance
        self.base_feed = feed
        self.convert_rows = convert_rows
        self.convert_cols = convert_cols
        self.cols_first_flag = cols_first_flag
        self.entry_cut = entry_cut
        self.spindle_speed = spindle_speed
        self.roughing_offset = roughing_offset
        self.roughing_delta = roughing_delta
        self.roughing_feed = roughing_feed

        self.cache = {}

        w, h = self.w, self.h = image.shape
        ts = self.ts = tool_shape.shape[0]

        self.h1 = h - ts
        self.w1 = w - ts

        self.tool_shape = tool_shape * self.pixelsize * ts / 2;
    
    def one_pass(self):
        g = self.g
        g.set_feed(self.feed)

        if self.convert_cols and self.cols_first_flag:
            self.g.set_plane(19)
            self.mill_cols(self.convert_cols, True)
            if self.convert_rows: g.safety()
        if self.convert_rows:
            self.g.set_plane(18)
            self.mill_rows(self.convert_rows, not self.cols_first_flag)
        if self.convert_cols and not self.cols_first_flag:
            self.g.set_plane(19)
            if self.convert_rows: g.safety()
            self.mill_cols(self.convert_cols, not self.convert_rows)
        if self.convert_cols:
            self.convert_cols.reset()
        if self.convert_rows:
            self.convert_rows.reset()
        g.safety()

    def convert(self):
        self.g = g = Gcode(safetyheight=self.safetyheight,
                           tolerance=self.tolerance,
                           spindle_speed=self.spindle_speed,
                           units=self.units)
        g.begin()
        g.continuous(self.tolerance)
        g.safety()
        if self.roughing_delta and self.roughing_offset:
            base_image = self.image
            rough = make_tool_shape(ball_tool,
                                2*self.roughing_offset, self.pixelsize)
            w, h = base_image.shape
            tw, th = rough.shape
            w1 = w + tw
            h1 = h + th
            nim1 = numpy.zeros((w1, h1), dtype=numpy.float32) + base_image.min()
            nim1[tw/2:tw/2+w, th/2:th/2+h] = base_image
            self.image = numpy.zeros((w,h), dtype=numpy.float32)
            for j in range(0, w):
                progress(j,w)
                for i in range(0, h):
                    self.image[j,i] = (nim1[j:j+tw,i:i+th] - rough).max()
            self.feed = self.roughing_feed
            r = -self.roughing_delta
            m = self.image.min()
            self.ro = self.roughing_offset
            while r > m:
                self.rd = r
                self.one_pass()
                r = r - self.roughing_delta
            if r < m + epsilon:
                self.rd = m
                self.one_pass()
            self.image = base_image
            self.cache.clear()
        self.feed = self.base_feed
        self.ro = 0
        self.rd = self.image.min()
        self.one_pass()
        g.end()

    def get_z(self, x, y):
        try:
            return min(0, max(self.rd, self.cache[x,y]) + self.ro)
        except KeyError:
            m1 = self.image[y:y+self.ts, x:x+self.ts]
            self.cache[x,y] = d = (m1 - self.tool).max()
            return min(0, max(self.rd, d) + self.ro)
        
    def get_dz_dy(self, x, y):
        y1 = max(0, y-1)
        y2 = min(self.image.shape[0]-1, y+1)
        dy = self.pixelsize * (y2-y1)
        return (self.get_z(x, y2) - self.get_z(x, y1)) / dy
        
    def get_dz_dx(self, x, y):
        x1 = max(0, x-1)
        x2 = min(self.image.shape[1]-1, x+1)
        dx = self.pixelsize * (x2-x1)
        return (self.get_z(x2, y) - self.get_z(x1, y)) / dx

    def mill_rows(self, convert_scan, primary):
        w1 = self.w1; h1 = self.h1;
        pixelsize = self.pixelsize; pixelstep = self.pixelstep
        jrange = range(0, w1, pixelstep)
        if w1-1 not in jrange: jrange.append(w1-1)
        irange = range(h1)

        for j in jrange:
            progress(jrange.index(j), len(jrange))
            y = (w1-j) * pixelsize
            scan = []
            for i in irange:
                x = i * pixelsize
                milldata = (i, (x, y, self.get_z(i, j)),
                    self.get_dz_dx(i, j), self.get_dz_dy(i, j))
                scan.append(milldata)
            for flag, points in convert_scan(primary, scan):
                if flag:
                    self.entry_cut(self, points[0][0], j, points)
                for p in points:
                    self.g.cut(*p[1])
            self.g.flush()

    def mill_cols(self, convert_scan, primary):
        w1 = self.w1; h1 = self.h1;
        pixelsize = self.pixelsize; pixelstep = self.pixelstep
        jrange = range(0, h1, pixelstep)
        irange = range(w1)
        if h1-1 not in jrange: jrange.append(h1-1)
        jrange.reverse()

        for j in jrange:
            progress(jrange.index(j), len(jrange))
            x = j * pixelsize
            scan = []
            for i in irange:
                y = (w1-i) * pixelsize
                milldata = (i, (x, y, self.get_z(j, i)),
                    self.get_dz_dy(j, i), self.get_dz_dx(j, i))
                scan.append(milldata)
            for flag, points in convert_scan(primary, scan):
                if flag:
                    self.entry_cut(self, j, points[0][0], points)
                for p in points:
                    self.g.cut(*p[1])
            self.g.flush()

def convert(*args, **kw):
    return Converter(*args, **kw).convert()

class SimpleEntryCut:
    def __init__(self, feed):
        self.feed = feed

    def __call__(self, conv, i0, j0, points):
        p = points[0][1]
        if self.feed:
            conv.g.set_feed(self.feed)
        conv.g.safety()
        conv.g.rapid(p[0], p[1])
        if self.feed:
            conv.g.set_feed(conv.feed)

def circ(r,b): 
    """\
Calculate the portion of the arc to do so that none is above the
safety height (that's just silly)"""

    z = r**2 - (r-b)**2
    if z < 0: z = 0
    return z**.5

class ArcEntryCut:
    def __init__(self, feed, max_radius):
        self.feed = feed
        self.max_radius = max_radius

    def __call__(self, conv, i0, j0, points):
        if len(points) < 2:
            p = points[0][1]
            if self.feed:
                conv.g.set_feed(self.feed)
            conv.g.safety()
            conv.g.rapid(p[0], p[1])
            if self.feed:
                conv.g.set_feed(conv.feed)
            return

        p1 = points[0][1]
        p2 = points[1][1]
        z0 = p1[2]

        lim = int(ceil(self.max_radius / conv.pixelsize))
        r = range(1, lim)

        if self.feed:
            conv.g.set_feed(self.feed)
        conv.g.safety()

        x, y, z = p1

        pixelsize = conv.pixelsize
        
        cx = cmp(p1[0], p2[0])
        cy = cmp(p1[1], p2[1])

        radius = self.max_radius

        if cx != 0:
            h1 = conv.h1
            for di in r:
                dx = di * pixelsize
                i = i0 + cx * di
                if i < 0 or i >= h1: break
                z1 = conv.get_z(i, j0)
                dz = (z1 - z0)
                if dz <= 0: continue
                if dz > dx:
                    conv.g.write("(case 1)")
                    radius = dx
                    break
                rad1 = (dx * dx / dz + dz) / 2
                if rad1 < radius:
                    radius = rad1
                if dx > radius:
                    break

            z1 = min(p1[2] + radius, conv.safetyheight)
            x1 = p1[0] + cx * circ(radius, z1 - p1[2])
            conv.g.rapid(x1, p1[1])
            conv.g.cut(z=z1)

            conv.g.flush(); conv.g.lastgcode = None
            if cx > 0:
                conv.g.write("G3 X%f Z%f R%f" % (p1[0], p1[2], radius))
            else:
                conv.g.write("G2 X%f Z%f R%f" % (p1[0], p1[2], radius))
            conv.g.lastx = p1[0]
            conv.g.lasty = p1[1]
            conv.g.lastz = p1[2]
        else:
            w1 = conv.w1
            for dj in r:
                dy = dj * pixelsize
                j = j0 - cy * dj
                if j < 0 or j >= w1: break
                z1 = conv.get_z(i0, j)
                dz = (z1 - z0)
                if dz <= 0: continue
                if dz > dy:
                    radius = dy
                    break
                rad1 = (dy * dy / dz + dz) / 2
                if rad1 < radius: radius = rad1
                if dy > radius: break

            z1 = min(p1[2] + radius, conv.safetyheight)
            y1 = p1[1] + cy * circ(radius, z1 - p1[2])
            conv.g.rapid(p1[0], y1)
            conv.g.cut(z=z1)

            conv.g.flush(); conv.g.lastgcode = None
            if cy > 0:
                conv.g.write("G2 Y%f Z%f R%f" % (p1[1], p1[2], radius))
            else:
                conv.g.write("G3 Y%f Z%f R%f" % (p1[1], p1[2], radius))
            conv.g.lastx = p1[0]
            conv.g.lasty = p1[1]
            conv.g.lastz = p1[2]
        if self.feed:
            conv.g.set_feed(conv.feed)

def ui(im, nim, im_name):
    import Tkinter
    import ImageTk
    import pickle
    import nf

    app = Tkinter.Tk()
    rs274.options.install(app)
    app.tk.call("source", os.path.join(BASE, "share", "axis", "tcl", "combobox.tcl"))

    name = os.path.basename(im_name)
    app.wm_title(_("%s: Image to gcode") % name)
    app.wm_iconname(_("Image to gcode"))
    w, h = im.size
    r1 = w / 300.
    r2 = h / 300.
    nw = int(w / max(r1, r2))
    nh = int(h / max(r1, r2))

    ui_image = im.resize((nw,nh), Image.ANTIALIAS)
    ui_image = ImageTk.PhotoImage(ui_image, master = app)
    i = Tkinter.Label(app, image=ui_image, compound="top",
        text=_("Image size: %(w)d x %(h)d pixels\n"
                "Minimum pixel value: %(min)d\nMaximum pixel value: %(max)d")
            % {'w': im.size[0], 'h': im.size[1], 'min': nim.min(), 'max': nim.max()},
        justify="left")
    f = Tkinter.Frame(app)
    g = Tkinter.Frame(app)
    b = Tkinter.Frame(app)
    i.grid(row=0, column=0, sticky="nw")
    f.grid(row=0, column=1, sticky="nw")
    b.grid(row=1, column=0, columnspan=2, sticky="ne")

    def filter_nonint(event):
        if event.keysym in ("Return", "Tab", "ISO_Left_Tab", "BackSpace"):
            return
        if event.char == "": return
        if event.char in "0123456789": return
        return "break"

    def filter_nonfloat(event):
        if event.keysym in ("Return", "Tab", "ISO_Left_Tab", "BackSpace"):
            return
        if event.char == "": return
        if event.char in "0123456789.": return
        return "break"
        
    validate_float    = "expr {![regexp {^-?([0-9]+(\.[0-9]*)?|\.[0-9]+|)$} %P]}"
    validate_int      = "expr {![regexp {^-?([0-9]+|)$} %P]}"
    validate_posfloat = "expr {![regexp {^?([0-9]+(\.[0-9]*)?|\.[0-9]+|)$} %P]}"
    validate_posint   = "expr {![regexp {^([0-9]+|)$} %P]}"
    def floatentry(f, v):
        var = Tkinter.DoubleVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_float, validate="key", width=10)
        return w, var

    def intentry(f, v):
        var = Tkinter.IntVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_int, validate="key", width=10)
        return w, var

    def checkbutton(k, v):
        var = Tkinter.BooleanVar(f)
        var.set(v)
        g = Tkinter.Frame(f)
        w = Tkinter.Checkbutton(g, variable=var, text=_("Yes"))
        w.pack(side="left")
        return g, var 

    def intscale(k, v, min=1, max = 100):
        var = Tkinter.IntVar(f)
        var.set(v)
        g = Tkinter.Frame(f, borderwidth=0)
        w = Tkinter.Scale(g, orient="h", variable=var, from_=min, to=max, showvalue=False)
        l = Tkinter.Label(g, textvariable=var, width=3)
        l.pack(side="left")
        w.pack(side="left", fill="x", expand=1)
        return g, var

    def _optionmenu(k, v, *options):
        options = list(options)
        def trace(*args):
            try:
                var.set(options.index(svar.get()))
            except ValueError:
                pass

        try:
            opt = options[v]
        except (TypeError, IndexError):
            v = 0
            opt = options[0]

        var = Tkinter.IntVar(f)    
        var.set(v)
        svar = Tkinter.StringVar(f)
        svar.set(options[v])
        svar.trace("w", trace)
        wp = f._w.rstrip(".") + ".c" + svar._name
        f.tk.call("combobox::combobox", wp, "-editable", 0, "-width",
                max(len(opt) for opt in options)+3, "-textvariable", svar._name,
                "-background", "white")
        f.tk.call(wp, "list", "insert", "end", *options)
        w = nf.makewidget(f, Tkinter.Widget, wp)
        return w, var

    def optionmenu(*options): return lambda f, v: _optionmenu(f, v, *options)

    rc = os.path.expanduser("~/.image2gcoderc")
    constructors = [
        ("units", optionmenu(_("G20 (in)"), _("G21 (mm)"))),
        ("invert", checkbutton),
        ("normalize", checkbutton),
        ("expand", optionmenu(_("None"), _("White"), _("Black"))),
        ("tolerance", floatentry),
        ("pixel_size", floatentry),
        ("feed_rate", floatentry),
        ("plunge_feed_rate", floatentry),
        ("spindle_speed", floatentry),
        ("pattern", optionmenu(_("Rows"), _("Columns"), _("Rows then Columns"), _("Columns then Rows"))),
        ("converter", optionmenu(_("Positive"), _("Negative"), _("Alternating"), _("Up Milling"), _("Down Milling"))),
        ("depth", floatentry),
        ("pixelstep", intscale),
        ("tool_diameter", floatentry),
        ("safety_height", floatentry),
        ("tool_type", optionmenu(_("Ball End"), _("Flat End"), _("30 Degree"), _("45 Degree"), _("60 Degree"))),
        ("bounded", optionmenu(_("None"), _("Secondary"), _("Full"))),
        ("contact_angle", floatentry),
        ("roughing_offset", floatentry),
        ("roughing_depth", floatentry),
    ]

    defaults = dict(
        invert = False,
        normalize = False,
        expand = 0,
        pixel_size = .006,
        depth = 0.25,
        pixelstep = 8,
        tool_diameter = 1/16.,
        safety_height = .012,
        tool_type = 0,
        tolerance = .001,
        feed_rate = 12,
        plunge_feed_rate = 12,
        units = 0,
        pattern = 0,
        converter = 0,
        bounded = 0,
        contact_angle = 45,
        spindle_speed = 1000,
        roughing_offset = .1,
        roughing_depth = .25,
    )

    texts = dict(
        invert=_("Invert Image"),
        normalize=_("Normalize Image"),
        expand=_("Extend Image Border"),
        pixel_size=_("Pixel Size (Units)"),
        depth=_("Depth (units)"),
        tolerance=_("Tolerance (units)"),
        pixelstep=_("Stepover (pixels)"),
        tool_diameter=_("Tool Diameter (units)"),
        tool_type=_("Tool Type"),
        feed_rate=_("Feed Rate (units per minute)"),
        plunge_feed_rate=_("Plunge Feed Rate (units per minute)"),
        units=_("Units"),
        safety_height=_("Safety Height (units)"),
        pattern=_("Scan Pattern"),
        converter=_("Scan Direction"),
        bounded=_("Lace Bounding"),
        contact_angle=_("Contact Angle (degrees)"),
        spindle_speed=_("Spindle Speed (RPM)"),
        roughing_offset=_("Roughing offset (units, 0=no roughing)"),
        roughing_depth=_("Roughing depth per pass (units)"),
    )

    try:
        defaults.update(pickle.load(open(rc, "rb")))
    except (IOError, pickle.PickleError): pass

    vars = {}
    widgets = {}
    for j, (k, con) in enumerate(constructors):
        v = defaults[k]
        text = texts.get(k, k.replace("_", " "))
        lab = Tkinter.Label(f, text=text)
        widgets[k], vars[k] = con(f, v)
        lab.grid(row=j, column=0, sticky="w")
        widgets[k].grid(row=j, column=1, sticky="ew")

    def trace_pattern(*args):
        if vars['pattern'].get() > 1:
            widgets['bounded'].configure(state="normal")
            trace_bounded()
        else:
            widgets['bounded'].configure(state="disabled")
            widgets['contact_angle'].configure(state="disabled")

    def trace_bounded(*args):
        if vars['bounded'].get() != 0:
            widgets['contact_angle'].configure(state="normal")
        else:
            widgets['contact_angle'].configure(state="disabled")

    def trace_offset(*args):
        if vars['roughing_offset'].get() > 0:
            widgets['roughing_depth'].configure(state='normal')
        else:
            widgets['roughing_depth'].configure(state='disabled')
    vars['pattern'].trace('w', trace_pattern)
    vars['bounded'].trace('w', trace_bounded)
    vars['roughing_offset'].trace('w', trace_offset)

    trace_pattern()
    trace_bounded()
    trace_offset()

    status = Tkinter.IntVar()
    bb = Tkinter.Button(b, text=_("OK"), command=lambda:status.set(1), width=8, default="active")
    bb.pack(side="left", padx=4, pady=4)
    bb = Tkinter.Button(b, text=_("Cancel"), command=lambda:status.set(-1), width=8, default="normal")
    bb.pack(side="left", padx=4, pady=4)
    
    app.bind("<Escape>", lambda evt: status.set(-1))
    app.bind("<Return>", lambda evt: status.set(1))
    app.wm_protocol("WM_DELETE_WINDOW", lambda: status.set(-1))
    app.wm_resizable(0,0)

    app.wait_visibility()
    app.tk.call("after", "idle", ("after", "idle", "focus [tk_focusNext .]"))
    #app.tk_focusNext().focus()
    app.wait_variable(status)


    for k, v in vars.items():
        defaults[k] = v.get()

    app.destroy()

    if status.get() == -1:
        raise SystemExit(_("image-to-gcode: User pressed cancel"))

    pickle.dump(defaults, open(rc, "wb"))

    return defaults

def main():
    if len(sys.argv) > 1:
        im_name = sys.argv[1]
    else:
        import tkFileDialog, Tkinter
        im_name = tkFileDialog.askopenfilename(defaultextension=".png",
            filetypes = (
                (_("Depth images"), ".gif .png .jpg"),
                (_("All files"), "*")))
        if not im_name: raise SystemExit
        Tkinter._default_root.destroy()
        Tkinter._default_root = None
    im = Image.open(im_name)
    size = im.size
    im = im.convert("L") #grayscale
    w, h = im.size

    nim = numpy.fromstring(tobytes(im), dtype=numpy.uint8).reshape((h, w)).astype(numpy.float32)
    options = ui(im, nim, im_name)

    step = options['pixelstep']
    depth = options['depth']

    if options['normalize']:
        a = nim.min()
        b = nim.max()
        if a != b:
            nim = (nim - a) / (b-a)
    else:
        nim = nim / 255.0

    maker = tool_makers[options['tool_type']]
    tool_diameter = options['tool_diameter']
    pixel_size = options['pixel_size']
    tool = make_tool_shape(maker, tool_diameter, pixel_size)

    if options['expand']:
        if options['expand'] == 1: pixel = 1
        else: pixel = 0
        w, h = nim.shape
        tw, th = tool.shape
        w1 = w + 2*tw
        h1 = h + 2*th
        nim1 = numpy.zeros((w1, h1), dtype=numpy.float32) + pixel
        nim1[tw:tw+w, th:th+h] = nim
        nim = nim1
        w, h = w1, h1
    nim = nim * depth

    if options['invert']:
        nim = -nim
    else:
        nim = nim - depth

    rows = options['pattern'] != 1
    columns = options['pattern'] != 0
    columns_first = options['pattern'] == 3
    spindle_speed = options['spindle_speed']
    if rows: convert_rows = convert_makers[options['converter']]()
    else: convert_rows = None
    if columns: convert_cols = convert_makers[options['converter']]()
    else: convert_cols = None

    if options['bounded'] and rows and columns:
        slope = tan(options['contact_angle'] * pi / 180)
        if columns_first:
            convert_rows = Reduce_Scan_Lace(convert_rows, slope, step+1)
        else:
            convert_cols = Reduce_Scan_Lace(convert_cols, slope, step+1)
        if options['bounded'] > 1:
            if columns_first:
                convert_cols = Reduce_Scan_Lace(convert_cols, slope, step+1)
            else:
                convert_rows = Reduce_Scan_Lace(convert_rows, slope, step+1)

    units = unitcodes[options['units']]
    convert(nim, units, tool, pixel_size, step,
        options['safety_height'], options['tolerance'], options['feed_rate'],
        convert_rows, convert_cols, columns_first, ArcEntryCut(options['plunge_feed_rate'], .125),
        spindle_speed, options['roughing_offset'], options['roughing_depth'], options['feed_rate'])

if __name__ == '__main__':
    main()

# vim:sw=4:sts=4:et:

