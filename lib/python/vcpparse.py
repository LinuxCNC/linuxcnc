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
import xml.dom.ext
from Tkinter import *
import sys, os
import emc
import pyvcp_widgets
import hal 
import time

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
    except:
        print "Error: could not open",filename,"!"
        sys.exit()
    print "Creating widgets from",filename,"...",
    # find the pydoc element
    for e in doc.childNodes:
        if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
            break

    if e.localName != "pyvcp":
        print "Error: no pyvcp element in file!"
        sys.exit()
    pyvcproot=e
    level=0
    nodeiterator(pyvcproot,pyvcp0,level) 
    print "Done."         



num=0
def nodeiterator(node,widgetparent,level):
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
            parent = str(e.parentNode.nodeName) + str(level)
            newwidget = widget_creator(widgetparent,e.nodeName,params,level+1)
            nodeiterator(e,newwidget,level+1)
      


widgets=[];
def widget_creator(parent,widget_name,params,level):
     """
        creates a pyVCP widget
            parent = parent widget
            widget_name = name of widget to be created 
            params = a list of parameters passed to the widget __init__
            level = what level in the dom tree are we at?
     """
  
     global widgets
     widgethandle= str(widget_name) + str(level)
   
     w_command=""
     w_command += "pyvcp_widgets.pyvcp_"+str(widget_name)+"("+"parent"+",pycomp"
     for a in params:
          w_command=w_command+","+str(a) 
     w_command=w_command+")"
     
     # this prints the statement generated above, 
     #print str(widgethandle) + " = " + w_command
     # and evaluated here:
     locals()[widgethandle] = eval(w_command)

     # pack the widget according to parent
     # either hbox or vbox
     if parent==pyvcp0:
          #print "this is the root element"
          pack_command = widgethandle +".pack(side='top',fill=X)"
     else:
          a= parent.packtype()
          if a == "top":
               pack_command = widgethandle +".pack(side='top',fill=X)"
          else:
               pack_command = widgethandle +".pack(side='left',fill=Y)"
     
     eval(pack_command)

     # add the widget to a global list widgets
     # to enable calling update() later
     list_command = 'widgets.append( ' + widgethandle + ' ) '
     eval(list_command)
     return_command = "return "+str(widgethandle) 
     
 
     return eval(str(widgethandle))



def paramiterator(node):
     """ returns a list of all parameters for a widget element """
     outparams =[]
     for e in node.childNodes:
          if e.nodeType == e.ELEMENT_NODE and (e.nodeName in pyvcp_widgets.parameters):
               outparams.append(str(e.nodeName) + \
                    " = " +  str(e.childNodes[0].nodeValue) )
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
    print "pyVCP:",
    read_file() 
    updater()
    return comp
    
if __name__ == '__main__':
    print "You can't run vcpparse.py by itself..."

    
    



