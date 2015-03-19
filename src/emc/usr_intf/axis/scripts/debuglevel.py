#!/usr/bin/python
import sys, os
import gettext
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
gettext.install("linuxcnc", localedir=os.path.join(BASE, "share", "locale"), unicode=True)

import linuxcnc
import Tkinter
import time

if len(sys.argv) > 1 and sys.argv[1] == '-ini':
    ini = linuxcnc.ini(sys.argv[2])
    nmlfile = ini.find("EMC", "NML_FILE")
    if nmlfile: linuxcnc.nmlfile = nmlfile
    del sys.argv[1:3]

s = linuxcnc.stat()
s.poll()
c = linuxcnc.command()

t = Tkinter.Tk(className="LinuxCNCDebugLevel")
t.wm_title(_("Machinekit Debug Level"))
t.wm_iconname(_("debuglevel"))
t.wm_resizable(0, 0)

import rs274.options
rs274.options.install(t)

# The configuration-related items are commented out because
# it only makes sense to enable them during startup.
#
# From inspection of the source, and because no messages were seen
# for those flags, it looks like the state of DEBUG_NML and DEBUG_RCS
# is only checked early in startup, so changing them later has no effect.
bits = [
    (linuxcnc.DEBUG_CONFIG, _('Configuration *')),
    (linuxcnc.DEBUG_VERSIONS, _('Version Numbers *')),
    (linuxcnc.DEBUG_NML, _('NML *')),
    (linuxcnc.DEBUG_RCS, _('RCS *')),
    (linuxcnc.DEBUG_TASK_ISSUE, _('Task Issue')),
    (linuxcnc.DEBUG_MOTION_TIME, _('Motion Time')),
    (linuxcnc.DEBUG_INTERP, _('Interpreter')),
    (linuxcnc.DEBUG_INTERP_LIST, _('Interpreter List')),
]

def showdebug(value):
    d.set(_("Inifile setting for this debug level:\n[EMC]DEBUG=0x%08x") % value)

vars = {}
blackout = 0
def update_buttons_from_emc():
    if time.time() < blackout:
        t.after(1000, update_buttons_from_emc)
        return
    try:
        s.poll()
    except linuxcnc.error: # linuxcnc exited?
        raise SystemExit
    debug = s.debug
    for k, v in vars.items():
        if debug & k: v.set(k)
        else: v.set(0)
    showdebug(debug)
    # .. just in case someone else changes it ..
    t.after(1000, update_buttons_from_emc)

def setdebug():
    blackout = time.time() + 1
    value = 0
    for k, v in vars.items():
        value = value | v.get()
    c.debug(value)
    showdebug(value)

for k, v in bits:
    vv = Tkinter.IntVar(t)
    vars[k] = vv
    b = Tkinter.Checkbutton(text = v, onvalue = k, offvalue = 0,
                        command=setdebug, variable=vv, anchor="nw")
    b.pack(side="top", anchor="nw", fill="x", expand=0)
l = Tkinter.Label(text=_("  * This option can only be enabled in the inifile"))
l.pack(side="top", anchor="nw")

d = Tkinter.StringVar(t)
l = Tkinter.Label(textvariable=d)
l.pack(side="top", anchor="nw")

showdebug(s.debug)

update_buttons_from_emc()

t.mainloop()
