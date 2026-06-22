#!/usr/bin/env python3
#
# Manual tool change UI — connects to manualtoolchange comp via REST API.
# Replaces the old hal_manualtoolchange.py which used HAL pins directly.
#
# Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
# License: GPL Version 2
#

import sys
import os
import gettext

BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
gettext.install("linuxcnc", localedir=os.path.join(BASE, "share", "locale"))

# Generated REST client (from manualtoolchange.gmi via gmicompile --client-python)
import gmi
from gmi.manualtoolchange_client import ManualtoolchangeClient

import nf
import rs274.options
import tkinter


def main():
    rest_url = gmi.rest_url()
    instance = gmi.mtc_instance()
    # The generated client builds base_url + "/api/v1/manualtoolchange",
    # but the REST server routes by instance name. Override the prefix
    # portion by constructing a URL that makes the client produce the
    # correct path: /api/v1/{instance}/...
    # We achieve this by letting the instance name replace the prefix
    # in the URL: the client appends "/api/v1/manualtoolchange" to
    # base_url, so we don't need to patch it if the instance is
    # registered with the API prefix as instance name.  Since modcompile
    # registers as "manualtoolchange.0", we adjust the base_url to
    # route correctly.
    base = rest_url
    # Override client's base_url after construction to use the right instance
    client = ManualtoolchangeClient(rest_url)
    client.base_url = f"{base}/api/v1/{instance}"

    app = tkinter.Tk(className="AxisToolChanger")
    app.wm_geometry("-60-60")
    app.wm_title(_("AXIS Manual Toolchanger"))
    rs274.options.install(app)
    nf.start(app)
    nf.makecommand(app, "_", _)
    app.wm_protocol("WM_DELETE_WINDOW", app.wm_withdraw)

    lab = tkinter.Message(app, aspect=500, text=_(
        "This window is part of the AXIS manual toolchanger.  It is safe to close "
        "or iconify this window, or it will close automatically after a few seconds."))
    lab.pack()

    def withdraw():
        app.wm_withdraw()
        app.bind("<Expose>", lambda event: app.wm_withdraw())

    app.after(10 * 1000, withdraw)

    prev_change = False

    def poll():
        nonlocal prev_change
        try:
            state = client.get_state()
        except Exception:
            # REST not ready yet or transient error — retry
            app.after(1000, poll)
            return

        if state.change_requested and not state.change_confirmed and not prev_change:
            prev_change = True
            do_change(app, client, state.tool_number)
        elif not state.change_requested:
            prev_change = False

        app.after(1000, poll)

    app.after(1000, poll)

    # Handle SIGTERM from launcher shutdown (via HAL UnloadAll).
    # Tkinter's mainloop swallows SystemExit raised by the default
    # SIGTERM handler, so we schedule app.destroy() from the signal.
    import signal
    def _on_sigterm(signum, frame):
        app.after(0, app.destroy)
    signal.signal(signal.SIGTERM, _on_sigterm)

    try:
        app.mainloop()
    except KeyboardInterrupt:
        pass


def do_change(app, client, tool_number):
    """Show tool change dialog, confirm via REST when user clicks Continue."""
    if tool_number:
        message = _("Insert tool %d and click continue when ready") % tool_number
    else:
        message = _("Remove the tool and click continue when ready")

    app.wm_withdraw()
    app.update()

    # Poll in background: if the change request disappears (e.g. change_button
    # confirmed it) dismiss the dialog automatically.
    dismissed = [False]

    def check_still_pending():
        if dismissed[0]:
            return
        try:
            state = client.get_state()
            if not state.change_requested or state.change_confirmed:
                # Already confirmed (e.g. via change_button pin) — dismiss dialog
                app.tk.call("set", "::tkPriv(button)", -1)
                dismissed[0] = True
                return
        except Exception:
            pass
        app.after(1000, check_still_pending)

    app.after(1000, check_still_pending)

    try:
        r = app.tk.call("nf_dialog", ".tool_change",
                         _("Tool change"), message, "info", 0, _("Continue"))
    finally:
        dismissed[0] = True

    if isinstance(r, str):
        r = int(r)
    if r == 0:
        try:
            client.confirm()
        except Exception:
            pass
    app.update()


if __name__ == "__main__":
    main()
