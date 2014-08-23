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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

"""
    Parses a pyVCP XML file and creates widgets by calling pyvcp_widgets.py
"""

import xml.dom.minidom
from Tkinter import *
import sys, os
import linuxcnc
import pyvcp_widgets
import hal 
import time
import traceback

# this statement is required so that stuff from Tkinter
# is not included in the pydoc documentation __All__ should list all 
# functions in this module
__all__=["read_file","nodeiterator",
        "widget_creator","paramiterator","updater","create_vcp"]




def read_file():
    """
        Reads the XML file specified by global 'filename'
        finds the <pyvcp> element and starts the parsing of the 
        file by calling nodeiterator()
    """
    try:
        doc = xml.dom.minidom.parse(filename) 
    except xml.parsers.expat.ExpatError, detail:
        print "Error: could not open",filename,"!"
	print detail
        sys.exit(1)
    # find the pydoc element
    for e in doc.childNodes:
        if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
            break

    if e.localName != "pyvcp":
        print "Error: no pyvcp element in file!"
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
      


widgets=[];
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
    except Exception, detail:
	raise SystemExit, "Error constructing %s(%s):\n%s" % (
			widget_name, params, detail)

    # pack the widget according to parent
    # either hbox or vbox
    if container==pyvcp0:
	widget.pack(side='top', fill=BOTH, expand=YES)
    else:
	parent.add(container, widget)
   
    # add the widget to a global list widgets
    # to enable calling update() later
    widgets.append(widget)

    return widget

def paramiterator(node):
    """ returns a list of all parameters for a widget element """
    outparams = {}
    for k, v in node.attributes.items():
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
                raise SystemExit, ("Error evaluating xml file:\n"
                    "Widget %s, Property %s\n%s: %s") % (
                        node.nodeName, e.nodeName, exc_type.__name__, exc_value)
	    outparams[str(e.nodeName)] = v
    return outparams



def updater():
     """ calls pyvcp_widgets.update() on each widget repeatedly every 100 ms """
     global widgets, pycomp
     for a in widgets:
          a.update(pycomp)
     pyvcp0.after(100,updater)




def create_vcp(master, comp = None, compname="pyvcp"):
    """ 
        create a pyVCP panel 
            master = Tkinter root window or other master container
            comp = HAL component
            compname = name of HAL component which is created if comp=None
    """
    global pyvcp0, pycomp
    pyvcp0 = master
    if comp is None:
        try: 
            comp = hal.component(compname)
        except:
            print "Error: Multiple components with the same name."
            sys.exit(0)

    pycomp = comp
    read_file() 
    updater()
    return comp
    
if __name__ == '__main__':
    print "You can't run vcpparse.py by itself..."

    
    



