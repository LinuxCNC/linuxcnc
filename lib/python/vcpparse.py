#    This is a component of AXIS, a front-end for emc
#    Copyright 2007 Anders Wallin <anders.wallin@helsinki.fi>
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

"""
    Parses a pyVCP XML panel definition and creates widgets via REST/WebSocket.
"""

import xml.dom.minidom
import sys
import pyvcp_widgets
from tkinter import *

__all__ = ["create_vcp_rest"]

_pyvcp0 = None


def create_vcp_rest(master, compname="pyvcp"):
    """
        Create a pyVCP panel using REST/WebSocket backend.

        Fetches panel XML and pin definitions from gomc-server,
        creates event-driven widgets connected via PyVCPClient.

        No polling loop. Server pushes state via WebSocket; widgets
        update displays via callbacks. User interactions send set_pin
        directly to server.

        Args:
            master: Tkinter root window or other master container
            compname: name of the panel instance on the server
    """
    from pyvcp_client import PyVCPClient, fetch_panel_info

    global _pyvcp0
    _pyvcp0 = master

    # Fetch panel info from server.
    info = fetch_panel_info(compname)

    # Create WebSocket client.
    client = PyVCPClient(compname, info["pins"], tk_root=master)

    # Parse XML from server and build widgets.
    _create_widgets_from_xml(info["xml"], master, client)

    # Start WebSocket — first update syncs all widget displays.
    client.start()
    client.wait_connected(timeout=5.0)

    return client


def _create_widgets_from_xml(xml_string, master, client):
    """Parse XML and create widgets using the client."""
    try:
        doc = xml.dom.minidom.parseString(xml_string)
    except xml.parsers.expat.ExpatError as detail:
        print("Error: could not parse XML string!")
        print(detail)
        sys.exit(1)
    for e in doc.childNodes:
        if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
            break
    if e.localName != "pyvcp":
        print("Error: no pyvcp element in XML!")
        sys.exit()
    _nodeiterator(e, master, client)


def _nodeiterator(node, widgetparent, client):
    """Recursive XML traversal creating widgets."""
    for e in node.childNodes:
        if e.nodeType == e.ELEMENT_NODE and (e.nodeName in pyvcp_widgets.elements):
            params = _paramiterator(e)
            newwidget = _widget_creator(widgetparent, e.nodeName, params, client)
            _nodeiterator(e, newwidget, client)


def _widget_creator(parent, widget_name, params, client):
    """Create a single widget."""
    constructor = getattr(pyvcp_widgets, "pyvcp_" + str(widget_name))
    if hasattr(parent, "getcontainer"):
        container = parent.getcontainer()
    else:
        container = parent
    positional_params = (container, client)

    try:
        widget = constructor(*positional_params, **params)
    except Exception as detail:
        raise SystemExit("Error constructing %s(%s):\n%s" % (widget_name, params, detail))

    # Pack the widget.
    if container == _pyvcp0:
        widget.pack(side='top', fill=BOTH, expand=YES)
    else:
        parent.add(container, widget)

    return widget


def _paramiterator(node):
    """Returns a dict of all parameters for a widget element."""
    outparams = {}
    for k, v in list(node.attributes.items()):
        if v and v[0] in "{[(\"'":
            v = eval(v)
        else:
            try:
                v = int(v)
            except ValueError:
                try:
                    v = float(v)
                except ValueError:
                    pass
        outparams[str(k)] = v

    for e in node.childNodes:
        if e.nodeType == e.ELEMENT_NODE \
                and (e.nodeName not in pyvcp_widgets.elements):
            try:
                v = eval(e.childNodes[0].nodeValue)
            except:
                exc_type, exc_value, exc_tb = sys.exc_info()
                raise SystemExit(("Error evaluating xml file:\n"
                    "Widget %s, Property %s\n%s: %s") % (
                        node.nodeName, e.nodeName, exc_type.__name__, exc_value))
            outparams[str(e.nodeName)] = v
    return outparams


# Public alias for use by pyvcp_include widget.
nodeiterator = _nodeiterator


if __name__ == '__main__':
    print("You can't run vcpparse.py by itself...")
