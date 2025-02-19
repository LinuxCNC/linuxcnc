#!/usr/bin/env python3
import sys, os
import gettext
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))

gettext.install("linuxcnc", localedir=os.path.join(BASE, "share", "locale"))

import linuxcnc, hal

_after = None
def hal_in_background():
    global _after
    _after = None
    if not h.change:
        app.tk.call("set", "::tkPriv(button)", -1)
        return

    if (h.change_button):
        h.changed = True
        app.update()
        app.tk.call("set", "::tkPriv(button)", -1)
        stop_polling_hal_in_background()
        return

    _after = app.after(100, hal_in_background)

def poll_hal_in_background():
    global _after
    _after = app.after(100, hal_in_background)

def stop_polling_hal_in_background():
    global _after
    if _after: app.after_cancel(_after)
    _after = None

def do_change(n):
    if n:
        message = get_tool_change_message(n)
    else:
        message = _("Remove the tool and click continue when ready")
    app.wm_withdraw()
    app.update()
    poll_hal_in_background()
    try:
        r = app.tk.call("nf_dialog", ".tool_change",
            _("Tool change"), message, "info", 0, _("Continue"))
    finally:
        stop_polling_hal_in_background()
    if isinstance(r, str): r = int(r)
    if r == 0:
        h.changed = True
    app.update()

def get_tool_change_message(n):
    try:
        s = linuxcnc.stat()
        s.poll()
        inipath = getattr(s, "ini_filename")
        inidir = os.path.dirname(inipath)
        inifile = linuxcnc.ini(inipath)
        machine_units = inifile.find("TRAJ", "LINEAR_UNITS")

        tool_info = s.toolinfo(n)
        comment = tool_info["comment"]
        diameter = tool_info["diameter"]

        return _("Insert tool and click continue when ready.\n\nTool number: %(number)s\nDiameter: %(diameter)s%(units)s\nComment: %(comment)s") % ({
            "number":   n,
            "diameter": diameter,
            "units":    machine_units,
            "comment":  comment
        })
    except Exception as error:
        # old style message with just tool number and the error message
        return "".join([_("Insert tool %d and click continue when ready") % n,
                        _("\n\nError: %s") % error])



h = hal.component("hal_manualtoolchange")
h.newpin("number", hal.HAL_S32, hal.HAL_IN)
h.newpin("change", hal.HAL_BIT, hal.HAL_IN)
h.newpin("change_button", hal.HAL_BIT, hal.HAL_IN)
h.newpin("changed", hal.HAL_BIT, hal.HAL_OUT)
h.ready()

import nf, rs274.options

import tkinter

app = tkinter.Tk(className="AxisToolChanger")
app.wm_geometry("-60-60")
app.wm_title(_("AXIS Manual Toolchanger"))
rs274.options.install(app)
nf.start(app); nf.makecommand(app, "_", _)
app.wm_protocol("WM_DELETE_WINDOW", app.wm_withdraw)
lab = tkinter.Message(app, aspect=500, text = _("\
This window is part of the AXIS manual toolchanger.  It is safe to close \
or iconify this window, or it will close automatically after a few seconds."))
lab.pack()

def withdraw():
    app.wm_withdraw()
    app.bind("<Expose>", lambda event: app.wm_withdraw())

app.after(10 * 1000, withdraw)

try:
    while 1:
        change = h.change
        if change and not h.changed:
            do_change(h.number)
        elif not change:
            h.changed = False
        app.after(100)
        app.update()
except KeyboardInterrupt:
    pass
