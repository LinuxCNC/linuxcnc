#!/usr/bin/python
import sys, os
import gettext
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
gettext.install("axis", localedir=os.path.join(BASE, "share", "locale"), unicode=True)

import emc
import Tkinter
import time

if len(sys.argv) > 1 and sys.argv[1] == '-ini':
    ini = emc.ini(sys.argv[2])
    emc.nmlfile = ini.find("EMC", "NML_FILE")
    del sys.argv[1:3]

s = emc.stat()
s.poll()
c = emc.command()

t = Tkinter.Tk(className="EmcDebugLevel")
t.wm_title("Debug Level")
t.wm_resizable(0, 0)

import rs274.options
rs274.options.install(t)

# The configuration-related items are commented out because
# it only makes sense to enable them during startup.
#
# From inspection of the source, and because no messages were seen
# for those flags, it looks like the state of DEBUG_NML and DEBUG_RCS
# is only checked early in startup, so changing them later has no effect.
#
# There's apparently only one message for EMC_DEBUG_IO_POINTS and hardly
# any messages for EMC_DEBUG_TRAJ (all from tkemc/mini), but I'll leave them
# enabled anyway
bits = [
    #(emc.DEBUG_INVALID, _('Configuration: Invalid items')),
    #(emc.DEBUG_CONFIG, _('Configuration: Default values')),
    #(emc.DEBUG_DEFAULTS, _('Configuration: Other')),
    #(emc.DEBUG_VERSIONS, _('Version Numbers')),
    (emc.DEBUG_TASK_ISSUE, _('Task Issue')),
    (emc.DEBUG_IO_POINTS, _('IO Points')),
    #(emc.DEBUG_NML, _('NML')),
    (emc.DEBUG_MOTION_TIME, _('Motion Time')),
    (emc.DEBUG_INTERP, _('Interpreter')),
    #(emc.DEBUG_RCS, _('RCS')),
    (emc.DEBUG_TRAJ, _('Trajectory')),
    (emc.DEBUG_INTERP_LIST, _('Interpreter List')),
]

vars = {}
blackout = 0
def update_buttons_from_emc():
    if time.time() < blackout: return
    try:
        s.poll()
    except emc.error: # emc exited?
        raise SystemExit
    debug = s.debug
    for k, v in vars.items():
        if debug & k: v.set(k)
        else: v.set(0)
    # .. just in case someone else changes it ..
    t.after(1000, update_buttons_from_emc)

def setdebug():
    blackout = time.time() + 1
    value = 0
    for k, v in vars.items():
        value = value | v.get()
    c.debug(value)

for k, v in bits:
    vv = Tkinter.IntVar(t)
    vars[k] = vv
    b = Tkinter.Checkbutton(text = v, onvalue = k, offvalue = 0,
                        command=setdebug, variable=vv)
    b.pack(side="top", anchor="nw")

update_buttons_from_emc()

t.mainloop()
