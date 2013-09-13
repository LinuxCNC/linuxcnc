import sys, os
BASE = os.environ['EMC2_HOME']
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import math

def _(s): return s

def ui():
    import Tkinter
    import pickle
    import nf
    import rs274.options
    import os

    app = Tkinter.Tk()
    rs274.options.install(app)
    app.tk.call("source", os.path.join(BASE, "share", "axis", "tcl", "combobox.tcl"))

    app.wm_title(_("Circular Holes"))
    app.wm_iconname(_("Circular Holes"))

    prev = Tkinter.Canvas(app, width=200, height=200)
    f = Tkinter.Frame(app)
    b = Tkinter.Frame(app)
    prev.grid(row=0, column=0, sticky="nw")
    f.grid(row=0, column=1, sticky="nw")
    b.grid(row=1, column=0, columnspan=2, sticky="ne")

    validate_float    = "expr {[regexp {^-?([0-9]+(\.[0-9]*)?|\.[0-9]+|)$} %P]}"
    validate_int      = "expr {[regexp {^-?([0-9]+|)$} %P]}"
    validate_posfloat = "expr {[regexp {^?([0-9]+(\.[0-9]*)?|\.[0-9]+|)$} %P]}"
    validate_posint   = "expr {[regexp {^([0-9]+|)$} %P]}"
    def posfloatentry(f, v):
        var = Tkinter.DoubleVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_posfloat, validate="all", width=10)
        return w, var

    def floatentry(f, v):
        var = Tkinter.DoubleVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_float, validate="all", width=10)
        return w, var

    def posintentry(f, v):
        var = Tkinter.IntVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_posint, validate="all", width=10)
        return w, var


    def intentry(f, v):
        var = Tkinter.IntVar(f)
        var.set(v)
        w = Tkinter.Entry(f, textvariable=var, validatecommand=validate_int, validate="all", width=10)
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

    rc = os.path.expanduser("~/.holecirclerc")
    constructors = [
        ("units", lambda f, v: optionmenu(f, v, _("G20 (in)"), _("G21 (mm)"))),
        ("cx", floatentry),
        ("cy", floatentry),
	("th0", floatentry),
	("inc", floatentry),
	("rad", posfloatentry),
	("count", posintentry),
	("feedrate", posfloatentry),
	("depth", floatentry),
	("dwell", posfloatentry),
	("retract", floatentry),
    ]

    defaults = dict(
	cx = 0,
	cy = 0,
	th0 = 0,
	inc = 15,
	count = 6,
	feedrate = 8,
	depth=-.1,
	retract=.1,
	units=0,
	dwell=0,
	rad=1
    )

    texts = dict(
        units=_("Units"),
	rad=_("Radius"),
	cx=_("Center X"),
	cy=_("Center Y"),
	th0=_("Start Angle"),
	inc=_("Increment Angle"),
	count=_("Hole Count"),
	feedrate=_("Feed Rate"),
	depth=_("Hole Depth"),
	retract=_("Retract Height"),
	dwell=("Dwell (0=no dwell)"),
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

    def update_preview(*args):
	prev.delete("all")
	try:
	    count = vars['count'].get()
	    th0 = vars['th0'].get()
	    inc = vars['inc'].get()
	except ValueError: return
	for i in range(count):
	    th = (th0 + i * inc) * math.pi / 180
	    x = 100 + 75 * math.cos(th)
	    y = 100 - 75 * math.sin(th)
	    prev.create_oval((x-4,y-4,x+4,y+4), fill='black')

    def update_ok(*args):
	result = True
	for i in vars.values():
	    try:
		i.get()
	    except ValueError:
		result = False
		break
	if result: bb.configure(state="normal")
	else: bb.configure(state="disabled")
	# This line creates an error when you load holecircle twice
	# from inside linuxcnc eg. gladevcp filechooser or AXIS GUI
	#print >>sys.stderr, "update_ok", args
    
    vars['count'].trace('w', update_preview)
    vars['inc'].trace('w', update_preview)
    vars['th0'].trace('w', update_preview)

    for i in vars.values(): i.trace('w', update_ok)

    update_preview()

    status = Tkinter.IntVar()
    bb = Tkinter.Button(b, text=_("OK"), command=lambda:status.set(1), width=8, default="active")
    bb.pack(side="left", padx=4, pady=4)
    bc = Tkinter.Button(b, text=_("Cancel"), command=lambda:status.set(-1), width=8, default="normal")
    bc.pack(side="left", padx=4, pady=4)
    
    app.bind("<Escape>", lambda evt: bc.invoke())
    app.bind("<Return>", lambda evt: bb.invoke())
    app.wm_protocol("WM_DELETE_WINDOW", lambda: bc.invoke())
    app.wm_resizable(0,0)

    app.wait_visibility()
    app.tk.call("after", "idle", ("after", "idle", "focus [tk_focusNext .]"))
    #app.tk_focusNext().focus()
    app.wait_variable(status)

    if status.get() == -1:
	raise SystemExit(1)

    for k, v in vars.items():
        defaults[k] = v.get()

    app.destroy()

    pickle.dump(defaults, open(rc, "wb"))

    return defaults

unitcodes = ['G20', 'G21']
u = ui()
print unitcodes[u['units']]
print "F%.1f" % u['feedrate']

count = u['count']
th0 = u['th0']
inc = u['inc']
depth = u['depth']
retract = u['retract']
cx = u['cx']
cy = u['cy']
rad = u['rad']

if u['dwell']: cycle = "G82 P% 8.4f" % u['dwell']
else: cycle = "G81"
for i in range(count):
    th = (th0 + i * inc) * math.pi / 180
    x = cx + rad * math.cos(th)
    y = cy + rad * math.sin(th)
    print "%s X% 8.4f Y% 8.4f Z% 8.4f R% 8.4f" % (cycle, x, y, depth, retract)
print "M2"
