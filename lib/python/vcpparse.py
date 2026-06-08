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
    Parses a pyVCP XML file and creates widgets by calling pyvcp_widgets.py
"""

import xml.dom.minidom
import io
import sys, os
import linuxcnc
import pyvcp_widgets
import time
import traceback
import tkinter as Tkinter
from tkinter import *
from importlib import reload

# this statement is required so that stuff from Tkinter
# is not included in the pydoc documentation __All__ should list all 
# functions in this module
__all__=["read_file","nodeiterator",
        "widget_creator","paramiterator","updater","create_vcp_rest"]




def read_file():
    """
        Reads the XML file specified by global 'filename'
        finds the <pyvcp> element and starts the parsing of the 
        file by calling nodeiterator()
    """
    try:
        doc = xml.dom.minidom.parse(filename) 
    except xml.parsers.expat.ExpatError as detail:
        print("Error: could not open",filename,"!")
        print(detail)
        sys.exit(1)
    # find the pydoc element
    for e in doc.childNodes:
        if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
            break

    if e.localName != "pyvcp":
        print("Error: no pyvcp element in file!")
        sys.exit()
    pyvcproot=e
    nodeiterator(pyvcproot,pyvcp0) 



num=0
def nodeiterator(node,widgetparent):
    """
        A recursive function that traverses the dom tree
        and calls widget_creator() when it finds a valid element
    """
    global num
    num+=1
    params=[]
    for e in node.childNodes:
        if e.nodeType == e.ELEMENT_NODE and (e.nodeName in pyvcp_widgets.elements):  
            params = paramiterator(e)  # find all the parameters for this node
            newwidget = widget_creator(widgetparent,e.nodeName,params)
            nodeiterator(e,newwidget)
      


widgets={}
def widget_creator(parent,widget_name,params):
    """
       creates a pyVCP widget
           parent = parent widget
           widget_name = name of widget to be created 
           params = a list of parameters passed to the widget __init__
    """
 
    global widgets
  
    constructor = getattr(pyvcp_widgets, "pyvcp_" + str(widget_name))
    if hasattr(parent, "getcontainer"):
        container = parent.getcontainer()
    else:
        container = parent
    positional_params = (container, pycomp)
    
    try:
        widget = constructor(*positional_params, **params)
    except Exception as detail:
        raise SystemExit("Error constructing %s(%s):\n%s" % (widget_name, params, detail))

    # pack the widget according to parent
    # either hbox or vbox
    if container==pyvcp0:
        widget.pack(side='top', fill=BOTH, expand=YES)
    else:
        parent.add(container, widget)
   
    # add the widget to a global list widgets
    # to enable calling update() later
    widgets[pycomp].append(widget)

    return widget

def paramiterator(node):
    """ returns a list of all parameters for a widget element """
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



def updater():
    """ calls pyvcp_widgets.update() on each widget repeatedly every 100 ms """
    global widgets, pycomp
    for w in widgets:
        for a in widgets[w]:
            a.update(w)
    pyvcp0.after(100,updater)




def read_xml_string(xml_string):
    """
        Parses an XML string (instead of a file) and creates widgets.
        Used by the REST/WebSocket mode where XML comes from the server.
    """
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
    nodeiterator(e, pyvcp0)


def create_vcp_rest(master, compname="pyvcp"):
    """
        Create a pyVCP panel using REST/WebSocket backend.

        Fetches panel XML and pin definitions from gomc-server,
        creates widgets using PyVCPCompat (drop-in hal.component replacement),
        and starts a WebSocket watch thread for pin updates.

        Args:
            master: Tkinter root window or other master container
            compname: name of the panel instance on the server
    """
    from gmi.pyvcp_compat import PyVCPCompat, fetch_panel_info

    reload(pyvcp_widgets)
    global pyvcp0, pycomp
    pyvcp0 = master

    # Fetch panel info from server.
    info = fetch_panel_info(compname)

    # Create compat layer (drop-in replacement for hal.component).
    comp = PyVCPCompat(compname, info["pins"])

    pycomp = comp
    widgets[pycomp] = []

    # Parse XML from server and build widgets.
    read_xml_string(info["xml"])

    # Start WebSocket connection for pin updates.
    comp.start()

    updater()
    return comp
    
if __name__ == '__main__':
    print("You can't run vcpparse.py by itself...")

    
    



