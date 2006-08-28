#!/usr/bin/python

## image-to-gcode is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2 of the License, or (at your
## option) any later version.  image-to-gcode is distributed in the hope 
## that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
## warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
## the GNU General Public License for more details.  You should have
## received a copy of the GNU General Public License along with image-to-gcode;
## if not, write to the Free Software Foundation, Inc., 59 Temple Place,
## Suite 330, Boston, MA 02111-1307 USA
## 
## image-to-gcode.py is Copyright (C) 2005 Chris Radek
## chris@timeguy.com
## image-to-gcode.py is Copyright (C) 2006 Jeff Epler
## jepler@unpy.net

import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import gettext;
gettext.install("axis", localedir=os.path.join(BASE, "share", "locale"), unicode=True)

import Image, numarray
import numarray.ieeespecial as ieee

from rs274.author import Gcode
import rs274.options

from math import *
import operator

def ball_tool(r):
    return 1-sqrt(1-r**2)

def endmill(r):
    return 0

def vee_common(angle):
    slope = tan(angle * pi / 180)
    def f(r):
        return r * slope
    return f
    
tool_makers = [ ball_tool, endmill, vee_common(45), vee_common(60)]

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

def make_tool_shape(f, dia):
    dia = int(dia+.5)
    n = numarray.array([[ieee.plus_inf] * dia] * dia, type="Float32")
    hdia = dia / 2.
    for x in range(dia):
        for y in range(dia):
            r = hypot(x-hdia, y-hdia) / hdia
            if r < 1:
                n[x,y] = f(r)
    return n

class Convert_Scan_Alternating:
    def __init__(self):
        self.st = 0

    def __call__(self, primary, items):
        st = self.st = self.st + 1
        if st % 2: items.reverse()
        yield False, items

class Convert_Scan_Increasing:
    def __call__(self, primary, items):
        yield True, items

class Convert_Scan_Decreasing:
    def __call__(self, primary, items):
        items.reverse()
        yield True, items

class Convert_Scan_Upmill:
    def __init__(self, slop = sin(pi / 18)):
        self.slop = slop

    def __call__(self, primary, items):
        for span in group_by_sign(items, self.slop, operator.itemgetter(2)):
            if amax([it[2] for it in span]) < 0:
                span.reverse()
            yield True, span

class Convert_Scan_Downmill:
    def __init__(self, slop = sin(pi / 18)):
        self.slop = slop

    def __call__(self, primary, items):
        for span in group_by_sign(items, self.slop, operator.itemgetter(2)):
            if amax([it[2] for it in span]) > 0:
                span.reverse()
            yield True, span

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
                        yield True, span[a:b+1]
                        a = None
            if a is not None:
                yield True, span[a:]

unitcodes = ['G20', 'G21']
convert_makers = [ Convert_Scan_Increasing, Convert_Scan_Decreasing, Convert_Scan_Alternating, Convert_Scan_Upmill, Convert_Scan_Downmill ]

def convert(image, units, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, convert_rows, convert_cols, cols_first_flag):
    w, h = image.shape
    tw, th = tool_shape.shape
    h1 = h - th
    w1 = w - tw
    tool_shape = tool_shape * pixelsize * tw / 2. 
    
    g = Gcode(safetyheight=safetyheight, tolerance=tolerance)
    g.begin()
    g.continuous()
    g.write(units)
    g.safety()
    g.set_feed(feed)
    if convert_cols and cols_first_flag:
        mill_cols(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, convert_cols, True)
        if convert_rows: g.safety()
    if convert_rows:
        mill_rows(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, convert_rows, not cols_first_flag)
    if convert_cols and not cols_first_flag:
        if convert_rows: g.safety()
        mill_cols(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, convert_cols, False)
    g.end()

cache = {}
def get_z(image, tool, x, y):
    try:
        return cache[x,y]
    except KeyError:
        tt = tool.shape[0]
        m1 = image[x:x+tt, y:y+tt]
        cache[x,y] = d = (m1 - tool).max()
        return d
        
def get_dz_dy(image, tool, x, y, pixelsize):
    y1 = max(0, y-1)
    y2 = min(image.shape[1]-1, y+1)
    dy = pixelsize * (y2-y1)
    return (get_z(image, tool, x, y2) - get_z(image, tool, x, y1)) / dy
        
def get_dz_dx(image, tool, x, y, pixelsize):
    x1 = max(0, x-1)
    x2 = min(image.shape[0]-1, x+1)
    dx = pixelsize * (x2-x1)
    return (get_z(image, tool, x2, y) - get_z(image, tool, x1, y)) / dx

def mill_cols(g, image, tool, pixelsize, pixelstep, safetyheight, tolerance, feed, convert_scan, primary):
    w, h = image.shape
    tw, th = tool.shape
    h1 = h - th
    w1 = w - tw
    jrange = range(0, w1, pixelstep)
    if w1-1 not in jrange: jrange.append(w1-1)
    irange = range(h1)

    for j in jrange:
        y = (w1-j) * pixelsize
        scan = []
        for i in irange:
            x = i * pixelsize
            milldata = i, (x, y, get_z(image, tool, i, j)), get_dz_dx(image, tool, i, j, pixelsize), get_dz_dy(image, tool, i, j, pixelsize)
            scan.append(milldata)
        for flag, points in convert_scan(primary, scan):
            if flag:
                g.safety()
                g.rapid(points[0][1][0], points[0][1][1])
            for p in points:
                g.cut(*p[1])

def mill_rows(g, image, tool, pixelsize, pixelstep, safetyheight, tolerance, feed, convert_scan, primary):
    w, h = image.shape
    tw, th = tool.shape
    h1 = h - th
    w1 = w - tw
    jrange = range(0, h1, pixelstep)
    irange = range(w1)
    irangerev = range(w1-1, -1, -1)
    if h1-1 not in jrange: jrange.append(h1-1)
    jrange.reverse()
    for j in jrange:
        x = j * pixelsize
        scan = []
        for i in irange:
            y = (w1-i) * pixelsize
            milldata = i, (x, y, get_z(image, tool, j, i)), get_dz_dy(image, tool, j, i, pixelsize), get_dz_dx(image, tool, j, i, pixelsize)
            scan.append(milldata)
        for flag, points in convert_scan(primary, scan):
            if flag:
                g.safety()
                g.rapid(points[0][1][0], points[0][1][1])
            for p in points:
                g.cut(*p[1])

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
        text=_("Image size: %d x %d pixels\n"
                "Minimum pixel value: %d\nMaximum pixel value: %d")
            % (im.size + (nim.min(), nim.max())),
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
        w = Tkinter.Checkbutton(g, variable=var, text="Yes")
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

    def optionmenu(k, v, *options):
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

    rc = os.path.expanduser("~/.image2gcoderc")
    constructors = [
        ("units", lambda f, v: optionmenu(f, v, _("G20 (in)"), _("G21 (mm)"))),
        ("invert", checkbutton),
        ("normalize", checkbutton),
        ("tolerance", floatentry),
        ("pixel_size", floatentry),
        ("feed_rate", floatentry),
        ("pattern", lambda f, v: optionmenu(f, v, _("Rows"), _("Columns"), _("Rows then Columns"), _("Columns then Rows"))),
        ("converter", lambda f, v: optionmenu(f, v, _("Positive"), _("Negative"), _("Alternating"), _("Up Milling"), _("Down Milling"))),
        ("depth", floatentry),
        ("pixelstep", intscale),
        ("tool_diameter", floatentry),
        ("safety_height", floatentry),
        ("tool_type", lambda f, v: optionmenu(f, v, _("Ball End"), _("Flat End"), _("45 Degree"), _("60 Degree"))),
        ("bounded", lambda f, v: optionmenu(f, v, _("None"), _("Secondary"), _("Full"))),
        ("contact_angle", floatentry),
    ]

    defaults = dict(
        invert = False,
        normalize = False,
        pixel_size = .006,
        depth = 0.25,
        pixelstep = 8,
        tool_diameter = 1/16.,
        safety_height = .012,
        tool_type = 0,
        tolerance = .001,
        feed_rate = 12,
        units = 0,
        pattern = 0,
        converter = 0,
        bounded = 0,
        contact_angle = 45,
    )

    texts = dict(
        invert=_("Invert Image"),
        normalize=_("Normalize Image"),
        pixel_size=_("Pixel Size"),
        depth=_("Depth (units)"),
        tolerance=_("Tolerance (units)"),
        pixelstep=_("Y step (pixels)"),
        tool_diameter=_("Tool Diameter (units)"),
        tool_type=_("Tool Type"),
        feed_rate=_("Feed Rate (units per minute)"),
        units=_("Units"),
        safety_height=_("Safety Height (units)"),
        pattern=_("Scan pattern"),
        converter=("Scan direction"),
        bounded=("Lace bounding"),
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
    vars['pattern'].trace('w', trace_pattern)
    vars['bounded'].trace('w', trace_bounded)

    trace_pattern()
    trace_bounded()

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

    if status.get() == -1: raise SystemExit(1)

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

    nim = numarray.fromstring(im.tostring(), 'UInt8', (h, w)).astype('Float32')
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

    nim = nim * depth

    if options['invert']:
        nim = -nim
    else:
        nim = nim - depth

    maker = tool_makers[options['tool_type']]
    tool_diameter = options['tool_diameter']
    pixel_size = options['pixel_size']
    tool = make_tool_shape(maker, tool_diameter / pixel_size)

    rows = options['pattern'] != 1
    columns = options['pattern'] != 0
    columns_first = options['pattern'] == 3
    if rows: convert_rows = convert_makers[options['converter']]()
    else: convert_rows = None
    if columns: convert_cols = convert_makers[options['converter']]()
    else: convert_cols = None

    if options['bounded'] and rows and columns:
        slope = tan(options['contact_angle'] * pi / 180)
        if columns_first:
            convert_rows = Reduce_Scan_Lace(convert_rows, slope, 3)            
        else:
            convert_cols = Reduce_Scan_Lace(convert_cols, slope, 3)            
        if options['bounded'] > 1:
            if columns_first:
                convert_cols = Reduce_Scan_Lace(convert_cols, slope, 3)            
            else:
                convert_rows = Reduce_Scan_Lace(convert_rows, slope, 3)            

    units = unitcodes[options['units']]
    convert(nim, units, tool, pixel_size, step,
        options['safety_height'], options['tolerance'], options['feed_rate'], convert_rows, convert_cols, columns_first)

if __name__ == '__main__':
    main()

# vim:sw=4:sts=4:et:

