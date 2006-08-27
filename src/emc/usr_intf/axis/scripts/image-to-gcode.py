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

def make_tool_shape(f, dia):
    dia = int(dia+.5)
    n = numarray.array([[ieee.plus_inf] * dia] * dia, type="Float64")
    hdia = dia / 2.
    for x in range(dia):
        for y in range(dia):
            r = hypot(x-hdia, y-hdia) / hdia
            if r < 1:
                n[x,y] = f(r)
    return n

def direction_alternating():
    while 1:
        yield True
        yield False

def direction_increasing():
    while 1:
        yield True

def direction_decreasing():
    while 1:
        yield False

unitcodes = ['G20', 'G21']
direction_makers = [ direction_increasing, direction_decreasing, direction_alternating ]

def convert(image, units, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, direction, rows_flag, cols_flag, cols_first_flag):
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
    if cols_flag and cols_first_flag:
        mill_cols(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, direction)
        if rows_flag: g.safety()
    if rows_flag:
        mill_rows(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, direction)
    if cols_flag and not cols_first_flag:
        if rows_flag: g.safety()
        mill_cols(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, direction)
    g.end()

def mill_cols(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, direction):
    w, h = image.shape
    tw, th = tool_shape.shape
    h1 = h - th
    w1 = w - tw
    jrange = range(0, w1, pixelstep)
    irange = range(h1)
    irangerev = range(h1-1, -1, -1)
    if w1-1 not in jrange: jrange.append(w1-1)
    jrange.reverse()
    for j, flag in zip(jrange, direction()):
        y = (w1-j) * pixelsize
        if flag:
            r = irange
        else:
            r = irangerev
        if j == jrange[0] or direction is not direction_alternating:
            g.rapid(r[0]*pixelsize, y)
        for i in r:
            x = i * pixelsize
            m1 = image[j:j+tw, i:i+tw]
            d = (m1 - tool_shape).max()
            g.cut(x, y, d)
        if direction is direction_alternating:
            g.flush()
            g.cut(y=y)
            g.flush()
        else:
            g.safety()

def mill_rows(g, image, tool_shape, pixelsize, pixelstep, safetyheight, tolerance, feed, direction):
    w, h = image.shape
    tw, th = tool_shape.shape
    h1 = h - th
    w1 = w - tw
    jrange = range(0, h1, pixelstep)
    irange = range(w1)
    irangerev = range(w1-1, -1, -1)
    if h1-1 not in jrange: jrange.append(h1-1)
    jrange.reverse()
    for j, flag in zip(jrange, direction()):
        x = j * pixelsize
        if flag:
            r = irange
        else:
            r = irangerev
        if j == jrange[0] or direction is not direction_alternating:
            g.rapid(x, (w1-r[0])*pixelsize)
        for i in r:
            y = (w1-i) * pixelsize
            m1 = image[i:i+tw, j:j+tw]
            d = (m1 - tool_shape).max()
            g.cut(x, y, d)
        if direction is direction_alternating:
            g.flush()
            g.cut(y=y)
            g.flush()
        else:
            g.safety()


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
        ("direction", lambda f, v: optionmenu(f, v, _("Positive"), _("Negative"), _("Alternating"))),
        ("depth", floatentry),
        ("pixelstep", intscale),
        ("tool_diameter", floatentry),
        ("safety_height", floatentry),
        ("tool_type", lambda f, v: optionmenu(f, v, _("Ball End"), _("Flat End"), _("45 Degree"), _("60 Degree")))
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
        direction = 0,
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
        direction=("Scan direction"),
    )

    try:
        defaults.update(pickle.load(open(rc, "rb")))
    except (IOError, pickle.PickleError): pass

    vars = {}
    for j, (k, con) in enumerate(constructors):
        v = defaults[k]
        text = texts.get(k, k.replace("_", " "))
        lab = Tkinter.Label(f, text=text)
        widget, vars[k] = con(f, v)
        lab.grid(row=j, column=0, sticky="w")
        widget.grid(row=j, column=1, sticky="ew")

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

    nim = numarray.fromstring(im.tostring(), 'UInt8', (h, w))
    options = ui(im, nim, im_name)

    step = options['pixelstep']
    depth = options['depth']

    
    nim = nim / 255.0

    if options['normalize']:
        a = nim.min()
        b = nim.max()
        if a != b:
            nim = (nim - a) / (b-a)

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
    direction = direction_makers[options['direction']]
    units = unitcodes[options['units']]
    convert(nim, units, tool, pixel_size, step,
        options['safety_height'], options['tolerance'], options['feed_rate'], direction, rows, columns, columns_first)

if __name__ == '__main__':
    main()

# vim:sw=4:sts=4:et:

