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

import Image, numarray
import numarray.ieeespecial as ieee

from rs274.author import Gcode

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
    
tool_makers = {
    "Ball": ball_tool,
    "Flat": endmill,
    "45 degree": vee_common(45),
    "60 degree": vee_common(60),
}

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

def convert(image, units, tool_shape, pixelsize, y_step, safetyheight, tolerance, feed):
    image = numarray.transpose(image)
    w, h = image.shape
    tw, th = tool_shape.shape
    h1 = h - th
    w1 = w - tw
    tool_shape = tool_shape * pixelsize * tw / 2. 
    #import sys
    #print >>sys.stderr, tool_shape
    
    g = Gcode(safetyheight=safetyheight, tolerance=tolerance)
    g.begin()
    g.continuous()
    g.write(units)
    g.safety()
    g.rapid(0,0)
    g.set_feed(feed)
    jrange = range(0, h1, y_step)
    irange = range(w1)
    irangerev = range(w1-1, -1, -1)
    if h1-1 not in jrange: jrange.append(h1-1)
    jrange.reverse()
    k = 0
    for j in jrange:
        k = k + 1
        y = (h1-j) * pixelsize
        if k%2==1:
            for i in irange:
                x = i * pixelsize
                m1 = image[i:i+tw, j:j+tw]
                d = (m1 - tool_shape).max()
                g.cut(x, y, d)
        else:
            for i in irangerev:
                x = i * pixelsize
                m1 = image[i:i+tw, j:j+tw]
                d = (m1 - tool_shape).max()
                g.cut(x, y, d)
        g.flush()
        g.cut(y=y)
        g.flush()

    g.end()

def ui(im, nim, im_name):
    import Tkinter
    import ImageTk
    import pickle

    app = Tkinter.Tk()

    app.option_add("*Entry.font", ("Helvetica", -12))
    app.option_add("*Menu.font", ("Helvetica", -12))
    app.option_add("*Menubutton.font", ("Helvetica", -12))
    app.option_add("*Label.font", ("Helvetica", -12))
    app.option_add("*Button.font", ("Helvetica", -12))
    app.option_add("*Radiobutton.font", ("Helvetica", -12))
    app.option_add("*Checkbutton.font", ("Helvetica", -12))
    app.option_add("*Scale.font", ("Helvetica", -12))

    name = os.path.basename(im_name)
    app.wm_title("%s: Image to gcode" % name)
    app.wm_iconname("Image to gcode")
    w, h = im.size
    r1 = w / 300.
    r2 = h / 300.
    nw = int(w / max(r1, r2))
    nh = int(h / max(r1, r2))

    ui_image = im.resize((nw,nh), Image.ANTIALIAS)
    ui_image = ImageTk.PhotoImage(ui_image, master = app)
    i = Tkinter.Label(app, image=ui_image, compound="top",
        text="Image size: %d x %d pixels\n"
                "Minimum pixel value: %d\nMaximum pixel value: %d"
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
        #w = Tkinter.Entry(f, textvariable=var)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_float, validate="key", width=10)
        #w.bind("<Key>", filter_nonfloat)
        return w, var

    def intentry(f, v):
        var = Tkinter.IntVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_int, validate="key", width=10)
        #w.bind("<Key>", filter_nonint)
        return w, var

    def checkbutton(k, v):
        var = Tkinter.BooleanVar(f)
        var.set(v)
        g = Tkinter.Frame(f)
        #b1 = Tkinter.Radiobutton(g, text="Yes", variable=var, value=True)
        #b2 = Tkinter.Radiobutton(g, text="No", variable=var, value=False)
        #b1.pack(side="left")
        #b2.pack(side="left")
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
        var = Tkinter.StringVar(f)
        var.set(v)
        w = Tkinter.OptionMenu(f, var, *options)
        w.configure(takefocus=1)
        return w, var

    rc = os.path.expanduser("~/.image2gcoderc")
    constructors = [
        ("units", lambda f, v: optionmenu(f, v, "G20 (in)", "G21 (mm)")),
        ("invert", checkbutton),
        ("normalize", checkbutton),
        ("tolerance", floatentry),
        ("pixel_size", floatentry),
        ("feed_rate", floatentry),
        ("depth", floatentry),
        ("y_step", intscale),
        ("tool_diameter", floatentry),
        ("safety_height", floatentry),
        ("tool_type", lambda f, v: optionmenu(f, v, *tool_makers.keys())),
    ]

    defaults = dict(
        invert = False,
        normalize = False,
        pixel_size = .006,
        depth = 0.25,
        y_step = 8,
        tool_diameter = 1/16.,
        safety_height = .012,
        tool_type = "Ball",
        tolerance = .001,
        feed_rate = 12,
        units = "G20 (in)"
    )

    texts = dict(
        invert="Invert Image",
        normalize="Normalize Image",
        pixel_size="Pixel Size",
        depth="Depth",
        tolerance="Tolerance",
        y_step="Y step",
        tool_diameter="Tool Diameter",
        tool_type="Tool Type",
        feed_rate="Feed Rate",
        units="Units",
        safety_height="Safety Height",
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
    bb = Tkinter.Button(b, text="OK", command=lambda:status.set(1), width=8, default="active")
    bb.pack(side="left", padx=4, pady=4)
    bb = Tkinter.Button(b, text="Cancel", command=lambda:status.set(-1), width=8, default="normal")
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
        im_name = tkFileDialog.askopenfilename(defaultextension=".png",
            filetypes = (
                ("Depth Images", ".gif .png .jpg"),
                ("All File", "*")))
        if not im: raise SystemExit
    im = Image.open(im_name)
    size = im.size
    im = im.convert("L") #grayscale
    w, h = im.size

    nim = numarray.fromstring(im.tostring(), 'UInt8', (h, w))
    options = ui(im, nim, im_name)

    step = options['y_step']
    depth = options['depth']

    
    nim = nim / 255.0

    if options['normalize']:
        a = nim.min()
        b = nim.max()
        print "(%f %f %f)" % (a, b, b-a)
        if a != b:
            nim = (nim - a) / (b-a)

    nim = nim * depth
    print "(%f %f)" % (nim.min(), nim.max())

    if options['invert']:
        nim = -nim
    else:
        nim = nim - depth

    maker = tool_makers[options['tool_type']]
    tool_diameter = options['tool_diameter']
    pixel_size = options['pixel_size']
    tool = make_tool_shape(maker, tool_diameter / pixel_size)
    convert(nim, options['units'], tool, pixel_size, options['y_step'],
        options['safety_height'], options['tolerance'], options['feed_rate'])

if __name__ == '__main__':
    main()

# vim:sw=4:sts=4:et:

