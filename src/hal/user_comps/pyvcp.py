#!/usr/bin/env python
#    This is a component of emc
#    Copyright 2007 Anders Wallin <anders.wallin@helsinki.fi>
#    
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


""" Python Virtual Control Panel for EMC

    A virtual control panel (VCP) is used to display and control
    HAL pins, which are either BIT or FLOAT valued.

    Usage: pyvcp -g WxH+X+Y -c compname myfile.xml

    compname is the name of the HAL component to be created. 
    The name of the HAL pins associated with the VCP will begin with 'compname.'
    
    myfile.xml is an XML file which specifies the layout of the VCP.
    Valid XML tags are described in the documentation for pyvcp_widgets.py

    -g option allows setting of the initial size and/or position of the panel
"""

import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import vcpparse
import hal
from Tkinter import Tk
import getopt

def usage():
    """ prints the usage message """
    print "Usage: pyvcp [-g WIDTHxHEIGHT+XOFFSET+YOFFSET][-c hal_component_name] myfile.xml"
    print "If the component name is not specified, the basename of the xml file is used."
    print "-g options are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen"
    print "use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position"

def main():
    """ creates a HAL component.
        calls vcpparse with the specified XML file.
    """
    try:
        opts, args = getopt.getopt(sys.argv[1:], "c:g:")
    except getopt.GetoptError, detail:
        print detail
        usage()
        sys.exit(1)
    window_geometry = None
    component_name = None
    for o, a in opts: 
        if o == "-c": 
            component_name = a
        if o == "-g": 
            window_geometry = a
        

    try:
        filename=args[0]
    except:
        usage()
        sys.exit(1)

    if component_name is None:
	component_name = os.path.splitext(os.path.basename(filename))[0]

    pyvcp0 = Tk()
    pyvcp0.title(component_name)
    if window_geometry:
        pyvcp0.geometry(window_geometry)
   
    vcpparse.filename=filename
    pycomp=vcpparse.create_vcp(compname=component_name, master=pyvcp0)
    pycomp.ready()

    try:
        try: 
            pyvcp0.mainloop()
        except KeyboardInterrupt:
            sys.exit(0)
    finally:
        pycomp.exit()

if __name__ == '__main__':
    main()
